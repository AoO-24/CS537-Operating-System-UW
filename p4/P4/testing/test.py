import subprocess, os, queue, sys, traceback, string
from time import time, sleep
import signal

class Failure(Exception):
   def __init__(self, value, detail=None):
      self.value = value
      self.detail = detail
   def __str__(self):
      if self.detail is not None:
         return str(self.value) + "\n" + str(self.detail)
      else:
         return str(self.value)

def addcslashes(s):
   special = {
      "\a":"\\a",
      "\r":"\\r",
      "\f":"\\f",
      "\n":"\\n",
      "\t":"\\t",
      "\v":"\\v",
      "\"":"\\\"",
      "\\":"\\\\"
      }
   r = ""
   for char in s:
      if char in special:
         r += special[char]
      elif char in string.printable:
         r += char
      else:
         r += "\\" + "{0:o}".format(ord(char))
   return r

def diff(expected, got):
   s = ""
   s += "Expected: \"" + addcslashes(expected) + "\"\n"
   s += "Got:      \"" + addcslashes(got) + "\""
   return s

class Test(object):

   IN_PROGRESS = 1
   PASSED = 2
   FAILED = 3

   name = None
   description = None
   command = None
   timeout = None
   children = list() # child processes
   point_value=0

   def __init__(self, project_path = None, test_path = None, log = None,
         use_gdb = False, use_valgrind = False):
      self.project_path = project_path
      self.logfd = log
      self.state = Test.IN_PROGRESS
      self.notices = list()
      self.use_gdb = use_gdb
      self.use_valgrind = use_valgrind
      self.test_path = test_path

   def is_failed(self):
      return self.state == Test.FAILED

   def is_passed(self):
      return self.state == Test.PASSED

   def fail(self, reason = None):
      self.state = Test.FAILED
      if reason is not None:
         self.notices.append(reason)

   def warn(self, reason):
      self.notices.append(reason)

   def done(self):
      self.logfd.flush()
      if not self.is_failed():
         self.state = Test.PASSED

   def __str__(self):
      s = "test " + self.name + " "
      if self.is_failed():
         s += "FAILED"
# Edit by Ceyhun
         #if self.point.value > 0:
         if self.point_value > 0:
            s+= " (0 of "+str(self.point_value)+")"
      elif self.is_passed():
         s += "PASSED"
         if self.point_value > 0:
            s+= " ("+str(self.point_value)+" of "+str(self.point_value)+")"
      s += "\n"
      s += " (" + self.description + ")\n"
      for note in self.notices:
         s += " " + note + "\n"

      if self.is_failed():
# Edited by Joe
         if self.command is not None:
            s += "\n Test it yourself by running\n"
            s += " " + self.command + "\n"
      return s
   
   def points(self):
      return self.point_value

   def runexe(self, args, libs = None, path = None,
         stdin = None, stdout = None, stderr = None, status = None, output=True):
      name = args[0]
      infd = None
      outfd = None
      errfd = None
      if stdin is not None:
         infd = subprocess.PIPE
      if stdout is not None:
         outfd = subprocess.PIPE
      if stderr is not None:
         errfd = subprocess.PIPE
      if output is not True:
         outfd = subprocess.PIPE

      start = time()
      child = self.startexe(args, libs, path,
            stdin=infd, stdout=outfd, stderr=errfd)
      self.children.append(child)
      (outdata, errdata) = child.communicate(stdin)
      child.wallclock_time = time() - start
      if status is not None and status != child.returncode:
         raise Failure(str(name) + " returned incorrect status code. " +
               "Expected " + str(status) + " got " + str(child.returncode))
      if stdout is not None and stdout != outdata:
         raise Failure(str(name) + " gave incorrect standard output.",
               diff(stdout, outdata))
      if stderr is not None and stderr != errdata:
         raise Failure(str(name) + " gave incorrect standard error.",
               diff(stderr, errdata))

      return child, outdata

   def startexe(self, args, libs = None, path = None,
         stdin = None, stdout = None, stderr = None, cwd = None):
      name = args[0]
      if stdout is None:
         stdout = self.logfd
      if stderr is None:
         stderr = self.logfd
      if path is None:
         path = os.path.join(self.project_path, name)
      if libs is not None:
         os.environ["LD_PRELOAD"] = libs
      if cwd is None:
         cwd = self.project_path
      args[0] = path
      self.log(" ".join(args))
      if self.use_gdb:
         child = subprocess.Popen(["xterm",
            "-title", "\"" + " ".join(args) + "\"",
            "-e", "gdb", "--args"] + args,
               cwd=cwd,
               stdin=stdin, stdout=stdout, stderr=stderr,
               shell=True)
         self.children.append(child)
         return child
      if self.use_valgrind:
         self.log("WITH VALGRIND")
         child = subprocess.Popen(["valgrind"] + args, # --log-fd=1
               stdin=stdin, stdout=stdout, stderr=stderr,
               cwd=cwd)
         sleep(1)
         self.children.append(child)
         return child
      else:
         child = subprocess.Popen(args, cwd=cwd,
               stdin=stdin, stdout=stdout, stderr=stderr)
         self.children.append(child)
         return child

      if libs is not None:
         os.environ["LD_PRELOAD"] = ""
         del os.environ["LD_PRELOAD"]

   # run a utility program in project directory
   def run_util(self, args):
      self.log(" ".join(args))
      child = subprocess.Popen(args, cwd=self.project_path, stdout=self.logfd,
            stderr=self.logfd)
      self.children.append(child)
      status = child.wait()
      return status

   # get ouput from utility program run in project directory
   def run_filter(self, args):
      self.log(" ".join(args))
      child = subprocess.Popen(args, cwd=self.project_path,
            stdout=subprocess.PIPE, stderr=None)
      stdoutdata = child.communicate()[0]
      if child.returncode != 0:
         raise Failure(args[0] + " failed", "Got error code " +
            str(child.returncode) + " from running '" + " ".join(args) + "'")
      return stdoutdata


   def after(self):
      pass

   def log(self, msg):
      self.logfd.write(msg + "\n")
      self.logfd.flush()

   def terminate(self):
      if not self.state == Test.FAILED:
         self.fail("terminated")
      for child in self.children:
         try:
            os.kill(child.pid, signal.SIGTERM)
         except OSError:
            pass

def quit_now(test):
   test.terminate()
   sys.exit()

def run_test(test, queue):
   signal.signal(signal.SIGTERM, lambda signum, frame: quit_now(test))
   # create a new process group so we can easily kill all children of this proc
   os.setpgrp()
   try:
      test.run()
   except Failure as f:
      test.fail(str(f.value))
      (type, value, tb) = sys.exc_info()
      # Don't print the stack trace, students find it unreadable
      traceback.print_exception(type, value, None, file=sys.stdout)
   except Exception as e:
      test.fail("Unexpected exception " + str(e))
      (type, value, tb) = sys.exc_info()
      # Don't print the stack trace, students find it unreadable
      traceback.print_exception(type, value, None, file=sys.stdout)
   finally:
      # Nullify test logfd for serialization
      test.logfd = None
      queue.put(test)
      sys.exit()
