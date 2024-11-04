import subprocess, os, glob

from testing import Test

# The build test is special in that it will try to build the project even if the
# test fails, so that other tests may be run
class BuildTest(Test):
   name = "build"
   description = "build project using make"
   timeout = 15
   targets = None

   def run(self):
      # self.clean(self.targets + ["*.o"])
      self.make(self.targets)
      self.done()

   def make(self, files=[], required=True):
      failures = list()
      self.log("Trying to build project using make")
      status = self.run_util(["make", "xv6.img", "fs.img"])
      if status != 0:
         failures.append("make failed (error " + str(status) + ")")
      missing_files = []
      for f in files:
         if not os.path.exists(self.project_path + "/" + f):
            failures.append(
                  "required file '" + f + "' does not exist after make")
            missing_files.append(f)
      if required:
         for failure in failures:
            self.fail(failure)
      return (len(failures) == 0)

   def clean(self, files=[], required=True):
      failures = list()
      warnings = list()
      self.log("Trying to clean project using make clean")
      status = self.run_util(["make", "clean"])
      for pattern in files:
         for f in glob.glob(pattern):
            if os.path.exists(self.project_path + "/" + f):
               warnings.append(
                     "file '" + f + "' exists after make clean, removing")
               # os.remove(self.project_path + "/" + f)
      if required:
         for failure in failures:
            self.fail(failure)
         for warning in warnings:
            self.warn(warning)
      return (len(failures) == 0)

