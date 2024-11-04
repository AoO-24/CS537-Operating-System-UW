#! /bin/env python

import toolspath
from testing import Xv6Build, Xv6Test

class test1(Xv6Test):
   name = "test_1"
   description = "Tests to make sure everything compiles"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test2(Xv6Test):
   name = "test_2"
   description = "Make sure default priority and nice are 0"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test3(Xv6Test):
   name = "test_3"
   description = "Test that nice works with valid arguments"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test4(Xv6Test):
   name = "test_4"
   description = "Test that nice fails for out of bounds arguments"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test5(Xv6Test):
   name = "test_5"
   description = "Test that getschedstate fails for null pointer"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test6(Xv6Test):
   name = "test_6"
   description = "Spinning process is assigned lower priority"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test7(Xv6Test):
   name = "test_7"
   description = "Two processes at same priority level should run in round-robin"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

# verify higher priority process is scheduled
class test8(Xv6Test):
   name = "test_8"
   description = "Process with higher priority after sleeping should be scheduled"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

# test priority with getschedstate()
class test9(Xv6Test):
   name = "test_9"
   description = "Process should have higher priority after sleeping"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test10(Xv6Test):
   name = "test_10"
   description = "Runnable process should be scheduled, even if it does not have highest priority"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test11(Xv6Test):
   name = "test_11"
   description = "Correct number of processes in use"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test12(Xv6Test):
   name = "test_12"
   description = "Test that getschedstate fails for bad pointer"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test13(Xv6Test):
   name = "test_13"
   description = "Three process workload for scheduler graph"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

class test14(Xv6Test):
   name = "test_14"
   description = "Process using nice should have lower priority"
   tester = "ctests/" + name + ".c"
   make_qemu_args = "CPUS=1"
   point_value = 1

import toolspath
from testing.runtests import main
main(Xv6Build, all_tests=[test1, test2, test3, test4, test5, test6, test7, test8, test9, test10, test11, test12, test13, test14])
