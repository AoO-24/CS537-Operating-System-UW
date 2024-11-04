# XV6 `getlastcat` System Call Implementation

**Name:** Wenpei Shao
**CS Login:** wenpei
**Wisc ID:** 9083215211
**Email:** wshao33@wisc.edu

## Implementation Status:

All features implemented and pass all test.

## Files Modified:

1. `defs.h`: Added the prototype for `getlastcat`.
2. `syscall.h`: Added the syscall number for `getlastcat`.
3. `syscall.c`: Added the syscall function to the syscall list.
4. `sysproc.c`: Implemented the `getlastcat` syscall.
5. `getlastcat.c`: Created a new user program to test the syscall.
6. `Makefile`: Added `getlastcat` to the `UPROGS` list.
7. `usys.S`: Added the syscall assembly code for getlastcat.
8. `sysfile.c`: Modify sys_open syscall funcation and add last_cat_filename extern char [] in order to record last time which file cat was call. Also Modify sys_exec syscall in order to put "No args were passed" when cat 0 was called.
9. `user.h`: Added the prototype for `getlastcat` user program.

## PS

When I try to use sys_exec to get the what argv are pass by cat. it is working and can pass my own operation on xv6 nrox. But this did not pass the tester. I have to switch to the sys_open in order to pass the tester.
