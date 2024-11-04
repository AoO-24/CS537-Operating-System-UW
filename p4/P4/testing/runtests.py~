import os, sys, tempfile, shutil, signal
from getopt import getopt
from optparse import OptionParser
import multiprocessing, queue
from time import sleep
from .test import *

parser = OptionParser(
      usage="Usage: %prog [options] tests...",
      description="Run specified tests on the project. "
      "By default, all tests are run.")
parser.add_option("-l", "--list",
      action="store_true", dest="print_list", default=False,
      help="Print a list of all available tests and their descriptions")
parser.add_option("-c", "--continue", action="store_true", dest="_continue",
      default=False, help="Continue testing after a test failure")
parser.add_option("-n", "--no-copy", action="store_false", dest="local",
      default=True,
      help="By default, the project is copied to a local temp directory "
      "before testing to avoid the poor performance of AFS. This option "
      "disables that behavior. This option along with --stop may be "
      "useful for debugging, since the project can be examined after a "
      "failed test")
parser.add_option("-p", "--project-path", dest="project_path", default=".",
      help="Path to the directory containing the project to be tested "
      "(default: current directory)")
parser.add_option("-t", "--test-path", dest="tester_path", default="tests",
      help="Path to the location of the test files")
parser.add_option("-q", "--quiet", action="store_true", dest="quiet",
      default=False)
parser.add_option("-m", "--no-timeout", action="store_true", dest="notimeout",
      default=False, help="Ignore timeouts on tests")
parser.add_option("-g", "--gdb", action="store_true", dest="gdb",
      default=False, help="Run project executable inside a gdb session. " +
      "implies -m")
parser.add_option("-v", "--valgrind", action="store_true", dest="valgrind",
      default=False, help="Run project executable inside a valgrind session. ")
parser.add_option("-b", "--no-build", action="store_false", dest="build",
      default=True,
      help="do not automatically run build test before running other tests")
parser.add_option("-f", "--factor", dest="factor", default=1,
      help="multiply all timeout lengths by FACTOR")

def simple_func(x, queue):
    queue.put('abc')
    sys.exit()

def main(build_test, all_tests):
   (options, args) = parser.parse_args()
   if options.gdb:
      options.notimeout = True

   tempdir = None
   if options.local:
      tempdir = tempfile.mkdtemp()
      if not options.print_list:
         shutil.copytree(src=options.project_path, dst=tempdir + "/p", symlinks=True)
      project_path = tempdir + "/p"
   else:
      project_path = options.project_path

   log = sys.stdout
   if options.quiet:
      log = open("/dev/null", "w")

   _list = list()
   if options.build:
      _list.append(build_test)
   if len(args) == 0:
      _list.extend(all_tests)
   for test_name in args:
      if test_name == "all":
         _list.extend(all_tests)
      if test_name == "build":
         _list.append(build_test)
      else:
         match = None
         for test in all_tests:
            if test.name == test_name:
               match = test
               break
         if match is not None:
            _list.append(match)
         else:
            sys.stderr.write(test_name + " is not a valid test\n")
            exit(2)


   if options.print_list:
      for test in _list:
         print(test.name, "-", test.description)
      sys.exit(0)

   ran = list()
   tests_passed = 0
   points = 0
   total_points = 0
   tests_skipped = 0
   quitnow = False
   for tester in _list:
      test = tester(project_path, log=log, use_gdb=options.gdb,
            use_valgrind=options.valgrind, test_path=options.tester_path)

      print('starting', test.name)

      log.write("\n")
      log.write("*" * 70 + "\n")
      log.write("\n")
      log.write("Test " + test.name + "\n")
      log.write(test.description + "\n")
      log.write("\n")
      log.write("*" * 70 + "\n")
      log.flush()

      # run the test in a new process
      result_queue = multiprocessing.Queue()
      p = multiprocessing.Process(target=run_test, args=(test,result_queue))
      p.start()
      if options.notimeout or test.timeout is None:
         timeout = None
      else:
         timeout = test.timeout * float(options.factor)
      try:
         # wait for the test result
         result = result_queue.get(True)
         p.join()
      except queue.Empty:
         test.fail("Timelimit (" + str(timeout) + "s) exceeded")
         result = test
      except KeyboardInterrupt:
         test.fail("User interrupted test")
         result = test
         quitnow = True
      finally:
         try:
            #os.killpg(os.getpgid(p.pid), signal.SIGTERM)
            os.kill(p.pid, signal.SIGTERM)
         except OSError as e:
            pass

      result_queue.close()

      try:
         result.logfd = log
         result.after()
      except Exception as e:
         (type, value, tb) = sys.exc_info()
         traceback.print_exception(type, value, tb)

      ran.append(result)
      total_points += test.points()

      log.flush()
      if not result.is_failed():
         points += test.points()
         tests_passed += 1

      log.write("\n")
      log.write("\n")
      log.write(str(result) + "\n")

      if result.is_failed() and not options._continue or quitnow:
         log.write("Skipped " + str(len(_list) - len(ran)) + " tests.\n")
         log.write("To keep testing after failing a test, use flag '-c' or '--continue'\n")
         sys.exit(1)
         

   log.write("*" * 70 + "\n")
   log.write("Summary:\n")


   for test in ran:
      log.write(str(test) + "\n")

   log.write("Passed " + str(tests_passed) + " of " + str(len(ran)) +
         " tests.\n")

   log.write("Overall " + str(tests_passed) + " of " + str(len(_list)) + "\n")

   if total_points > 0:
      log.write("Points " + str(points) + " of " + str(total_points) + "\n")

   if options.quiet:
      for test in ran:
         print(str(test))

      print("Overall " + str(tests_passed) + " of " + str(len(_list)))
      if total_points > 0:
         print("Points " + str(points) + " of " + str(total_points))

   if tempdir is not None:
      shutil.rmtree(tempdir)

   if tests_passed == len(_list):
      sys.exit(0)
   else:
      sys.exit(1)

if __name__ == "__main__":
    main()
