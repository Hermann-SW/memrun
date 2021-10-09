# memrun
Small tool written in C to run ELF binaries (verified on x86_64 and armv7l), either from standard input, or via first argument process substitution. Works on Linux where kernel version is >= 3.17 (relies on the `memfd_create` syscall).


# Usage

Prepare with executing `runme` once. Allows to run C source compiled with gcc passed via pipe to memrun without temporary filesystem files ("tcc -run" equivalent). Anonymous file created and executed lives in RAM, without link in filesystem. If you are not interested in [memrun.c](memrun.c) details, you can skip discussion wrt [info.c](info.c), and continue with [C script](#C-script) discussion.

Here gcc compiled ELF output gets stored in stdout (file descriptor 1), and piped to memrun that executes it:
```
pi@raspberrypi400:~/memrun/C $ gcc info.c -o /dev/fd/1 | ./memrun
My process ID : 20043
argv[0] : ./memrun
no argv[1]
evecve --> /usr/bin/ls -l /proc/20043/fd
total 0
lr-x------ 1 pi pi 64 Sep 18 22:27 0 -> 'pipe:[1601148]'
lrwx------ 1 pi pi 64 Sep 18 22:27 1 -> /dev/pts/4
lrwx------ 1 pi pi 64 Sep 18 22:27 2 -> /dev/pts/4
lr-x------ 1 pi pi 64 Sep 18 22:27 3 -> /proc/20043/fd
pi@raspberrypi400:~/memrun/C $ 
```

There is a simple reason why the memfd file descriptor is not visible. clang-tidy raised warning "'memfd_create' should use MFD_CLOEXEC where possible" when passing 0 as 2nd argument of "memfd_create()". When executable filename contains '_' character, intentionally 0 gets passed, and memfd gets not closed and is visible:
```
pi@raspberrypi400:~/memrun/C $ ln -s memrun memrun_
pi@raspberrypi400:~/memrun/C $ gcc info.c -o /dev/fd/1 | ./memrun_
My process ID : 20098
argv[0] : ./memrun_
no argv[1]
evecve --> /usr/bin/ls -l /proc/20098/fd
total 0
lr-x------ 1 pi pi 64 Sep 18 22:27 0 -> 'pipe:[1601207]'
lrwx------ 1 pi pi 64 Sep 18 22:27 1 -> /dev/pts/4
lrwx------ 1 pi pi 64 Sep 18 22:27 2 -> /dev/pts/4
lrwx------ 1 pi pi 64 Sep 18 22:27 3 -> '/memfd:foo.bar (deleted)'
lr-x------ 1 pi pi 64 Sep 18 22:27 4 -> /proc/20098/fd
pi@raspberrypi400:~/memrun/C $ 
```

## C script

Previous method to run ELF file via pipe makes it impossible for compiled C code to access "C script" stdin. Running compiled C code via first argument process substitution resolves the problem. gcc creates ELF to stdout, and since it cannot determine language from file name, "-x c" specifies it. 2nd process substitution extracts C code from "C script" [run_from_memory_stdin.c](run_from_memory_stdin.c):
```
...
echo "foo"
./memrun <(gcc -o /dev/fd/1 -x c <(sed -n "/^\/\*\*$/,\$p" $0)) 42
...
```

Here you see that this method allows to pass 42 as argv[1] to execution. C code does output stdin for verification (that really C script stdin is accessible to C code):
```
pi@raspberrypi400:~/memrun/C $ grep mfd memrun.c | ./run_from_memory_stdin.c 
foo
bar 42
void exc(int mfd, char *argv[], char *envp[]);
  int mfd = memfd_create("foo.bar", MFD_CLOEXEC);
  cpyfd(mfd, fd);
  exc(mfd, argv, argv+argc);
void exc(int mfd, char *argv[], char *envp[])
  sprintf(buf+14, "%d", mfd);
pi@raspberrypi400:~/memrun/C $ 
```

18 lines only C script [run_from_memory_stdin.c](run_from_memory_stdin.c) (it has to be executable!) shows how the previous output was created:

```
#!/bin/bash

echo "foo"
./memrun <(gcc -o /dev/fd/1 -x c <(sed -n "/^\/\*\*$/,\$p" $0)) 42

exit
/**
*/
#include <stdio.h>

int main(int argc, char *argv[])
{
  printf("bar %s\n", argc>1 ? argv[1] : "(undef)");

  for(int c=getchar(); EOF!=c; c=getchar())  { putchar(c); }

  return 0;
}
```

## C++ script

For those who prefer C++ over C, very few changes are needed for "C++ script" [run_from_memory_cin.cpp](run_from_memory_cin.cpp).
This is the only "run from memory" solution for C++ ("tcc -run" cannot help, because tcc is a C compiler only):  
```
pi@raspberrypi400:~/memrun/C $ diff run_from_memory_stdin.c run_from_memory_cin.cpp
4c4
< ./memrun <(gcc -o /dev/fd/1 -x c <(sed -n "/^\/\*\*$/,\$p" $0)) 42
---
> ./memrun <(g++ -o /dev/fd/1 -x c++ <(sed -n "/^\/\*\*$/,\$p" $0)) 42
9c9
< #include <stdio.h>
---
> #include <iostream>
15c15
<   for(int c=getchar(); EOF!=c; c=getchar())  { putchar(c); }
---
>   for(char c; std::cin.read(&c, 1); )  { std::cout << c; }
pi@raspberrypi400:~/memrun/C $ 
```

## C/C++ interaction with bash in script

The C/C++ scripts seen sofar only showed how bash and C/C++ in single file can work.
2 years ago I created [4DoF robot arm](https://github.com/Hermann-SW/4DoF_robot_arm) repo.
That repo's "gamepad" tool is a C script, with executable created under "/tmp".
Its C code (Jason White's joystick.c from another repo) did low level gamepad event processing.
The bash code used the processed events to control 4 servo motors with pigpio library pigs commands.
Interaction was done, in that the stdout output of C code was input for the bash script.

I copied that tool over as [gamepad.c](gamepad.c) C script, and modified it to "run from memory", with minimal diff:  
```
pi@raspberrypi400:~/memrun/C $ diff ~/4DoF_robot_arm/tools/gamepad gamepad.c 
7,9d6
< js=/tmp/joystick
< if [ ! -f $js ]; then sed -n "/^\/\*\*$/,\$p" $0 | gcc -x c - -o $js; fi
< 
87c84
< done < <($js)
---
> done < <(./memrun <(gcc -o /dev/fd/1 -x c <(sed -n "/^\/\*\*$/,\$p" $0)))
pi@raspberrypi400:~/memrun/C $ 
```

This is how bash and C code parts work together:
```
...
while IFS= read -r line; do
    case $line in
        ...
    esac
    ...
    echo $g $u $l $p $d $calib

    pigs s 8 $g; pigs s 9 $u; pigs s 10 $l; pigs s 11 $p

done < <(./memrun <(gcc -o /dev/fd/1 -x c <(sed -n "/^\/\*\*$/,\$p" $0)))

exit
/**
 * Author: Jason White
...  
```


## Adding (tcc) "-run" option to gcc and g++

The symbolic links bin/gcc and bin/g++ to [bin/grun](bin/grun)
add tcc's "-run" option to gcc and g++. grun tests
whether "-run" option is present. If not normal gcc 
and g++ will be invoked as always. Otherwise gcc/g++
is used to compile into RAM, and memrun executes from there ([memrun.c](memrun.c) gets compiled automatically if needed).
The syntax is the same as tcc's syntax:
```
pi@raspberrypi400:~/memrun/C $ tcc | grep "\-run"
       tcc [options...] -run infile [arguments...]
  -run        run compiled source
pi@raspberrypi400:~/memrun/C $ 
```

First bin/gcc and bin/g++ need to be used instead of normal gcc and g++ (by preprending bin to $PATH):
```
pi@raspberrypi400:~/memrun/C $ which gcc
/usr/bin/gcc
pi@raspberrypi400:~/memrun/C $ export PATH=~/memrun/C/bin:$PATH
pi@raspberrypi400:~/memrun/C $ which gcc
/home/pi/memrun/C/bin/gcc
pi@raspberrypi400:~/memrun/C $ which g++
/home/pi/memrun/C/bin/g++
pi@raspberrypi400:~/memrun/C $ 
```

In case no "-run" is found, normal gcc is used ...
```
pi@raspberrypi400:~/memrun/C $ ls a.out 
ls: cannot access 'a.out': No such file or directory
pi@raspberrypi400:~/memrun/C $ gcc info.c 
pi@raspberrypi400:~/memrun/C $ ls a.out 
a.out
pi@raspberrypi400:~/memrun/C $
```

... as well as normal g++:
```
pi@raspberrypi400:~/memrun/C $ rm a.out 
pi@raspberrypi400:~/memrun/C $ sed -n "/^\/\*\*$/,\$p" run_from_memory_cin.cpp > demo.cpp
pi@raspberrypi400:~/memrun/C $ g++ demo.cpp 
pi@raspberrypi400:~/memrun/C $ ls a.out 
a.out
pi@raspberrypi400:~/memrun/C $ 
```


In case "-run" is present, gcc ...
```
pi@raspberrypi400:~/memrun/C $ uname -a | gcc -O3 -run info.c test
My process ID : 24787
argv[0] : /home/pi/memrun/C/bin/../memrun
argv[1] : test

evecve --> /usr/bin/ls -l /proc/24787/fd
total 0
lr-x------ 1 pi pi 64 Sep 20 00:48 0 -> 'pipe:[3396870]'
lrwx------ 1 pi pi 64 Sep 20 00:48 1 -> /dev/pts/0
lrwx------ 1 pi pi 64 Sep 20 00:48 2 -> /dev/pts/0
lr-x------ 1 pi pi 64 Sep 20 00:48 3 -> /proc/24787/fd
lr-x------ 1 pi pi 64 Sep 20 00:48 63 -> 'pipe:[3394399]'
pi@raspberrypi400:~/memrun/C $
```

... as well as g++ behave like tcc with "-run" (compile to and execute from RAM, no storing of executable in filesystem):
```
pi@raspberrypi400:~/memrun/C $ uname -a | g++ -O3 -Wall -run demo.cpp 42
bar 42
Linux raspberrypi400 5.10.60-v7l+ #1449 SMP Wed Aug 25 15:00:44 BST 2021 armv7l GNU/Linux
pi@raspberrypi400:~/memrun/C $ 
```

## Shebang processing

Like tcc, "-run" enabled gcc and g++ can be invoked from scripts (by [shebang](https://en.wikipedia.org/wiki/Shebang_(Unix)) character sequence).

gcc example:
```
pi@raspberrypi400:~/memrun/C $ ./HelloWorld.c
Hello, World!
pi@raspberrypi400:~/memrun/C $ cat HelloWorld.c
#!/home/pi/memrun/C/bin/gcc -run
#include <stdio.h>

int main(void)
{
  printf("Hello, World!\n");
  return 0;
}
pi@raspberrypi400:~/memrun/C $ 
```

g++ example:
```
pi@raspberrypi400:~/memrun/C $ ./HelloWorld.cpp 
Hello, World!
pi@raspberrypi400:~/memrun/C $ cat HelloWorld.cpp 
#!/home/pi/memrun/C/bin/g++ -run
#include <iostream>

int main(void)
{
  std::cout << "Hello, World!" << std::endl;
  return 0;
}
pi@raspberrypi400:~/memrun/C $ 
```

## memfd_create for bash

While memrun.c is fine, it does three things:
1. create memory file
2. copy compiled executable to memory file
3. execute memory file

2 and 3 can be done in bash or on command line. New [memfd_create.c](memfd_create.c) just exposes memfd_create system call for use with bash. After creating memory file, it creates the proc name for accessing the memory file and prints it to the calling process. That way bash script gets name for using the memory file. Memory file lives as long as memfd_create is running (it sleeps while waiting to be terminated). Bash script needs to extract pid from file name and just kill that pid, then memory file will not be accessible anymore.

Example script [memfd_create4bash](memfd_create4bash) demonstrates all that; this is demo output:
```
pi@raspberrypi400:~/memrun/C $ ./memfd_create4bash 
/proc/1808/fd/3
foo
bar
cat: /proc/1808/fd/3: No such file or directory
pi@raspberrypi400:~/memrun/C $ 
```

[memfd_create_fs_demo](memfd_create_fs_demo) demonstrates creation of filesystem in created memory file, and mounting that under "/proc/PID/fd" for process ID "PID". No access to real filesystem is needed:
```
pi@raspberrypi400:~/memrun/C $ ./memfd_create_fs_demo 
/proc/13674/fd
total 13
-rw-r--r-- 1 pi   pi       4 Sep 30 12:22 bar
drwx------ 2 root root 12288 Sep 30 12:22 lost+found
foo
pi@raspberrypi400:~/memrun/C $ 
```

## Now "-run" enabled gcc and g++ run completely from RAM

gcc/g++ creates some temporary files during compilation, that by default end up in "/tmp", which is not even a tmpfs under Raspberry Pi 32bit OS. For details see [this posting and following](https://www.raspberrypi.org/forums/viewtopic.php?f=31&t=320170&p=1918904#p1917142).

In order to run completely from RAM, a directory in RAM is needed for temporary files.  With [this commit](https://github.com/Hermann-SW/memrun/commit/5a2444ed5a82e562a8511f6a8a4201c5089dddf1) "/bin/grun" (bin/gcc and bin/g++ link to that file) does

- create memory file
- create filesyystem in memory file
- mount that filesystem (under "/proc/PID/fd")
- create executable "doit" in that filesystem, inclusive all temporary files
- execute "doit"
- release memory file&filesystem

So now "-run" enabled gcc/g++ runs completely from RAM!

## mount_tmpfs technique works without memfd_create system call

Previous work in ths repo made use of memfd_create system call, first to create a single memory file, later to create a filesystem in that memory file. I will keep previous tools and examples for using memfd_create system call.  

I learned from [*Uncle Billy* on unix.stackexchange.com](https://unix.stackexchange.com/questions/672179/linux-mount-point-in-proc-virtual-filesystem) how to mount a memory filesystem easier than I did with just "mount -t tmpfs ...". New implementation of "-run" enabled gcc/g++ and examples how to use are in [mount_tmpfs](mount_tmpfs) directory.

Like before temporary memory directory gets mounted under "/proc/$$/fd" in process specific directory ($$ is current process id). In case that fails, message is generated that "/tmp/$$" workarodund gets used. This happend under RHEL selinux. In case you see the workarodund message, you can make selinux accept mounting under "/proc/$$/fd" using these commands once:  
```
sudo ausearch -c 'mount' --raw | grep denied | tail | audit2allow -M my-mount
semodule -i my-mount.pp
```

Different to before, now grun (and gcc/g++ links to grun) get stored under "/user/local/bin". This allows same access path for all users for "-run" enabled gcc/g++ in C/C++ script shebang. You do that by executing [mount_tmpfs/_install](mount_tmpfs/_install) (from anywhere). In case you want to undo that, execute [mount_tmpfs/_uninstall](mount_tmpfs/_uninstall).

Executable C++ script [mount_tmpfs/HelloWorld.cpp](mount_tmpfs/HelloWorld.cpp) uses shebang "#!/usr/local/bin/g++ -run", and prints read input besides "Hello ...":
```
pi@raspberrypi400:~/memrun/C/mount_tmpfs $ fortune -s | ./HelloWorld.cpp 
Hello, World!
Speer's 1st Law of Proofreading:
	The visibility of an error is inversely proportional to the
	number of times you have looked at it.
pi@raspberrypi400:~/memrun/C/mount_tmpfs $ 
```

Interaction between bash and C++ in bash script are demonstrated in [mount_tmpfs/run_from_memory_cin.cpp](mount_tmpfs/run_from_memory_cin.cpp). Here bash reads C++ output, and prints it "C: " prefixed. Bash passes "42" as first arg to C++ code, which prints it:
```
pi@raspberrypi400:~/memrun/C/mount_tmpfs $ uname -o | ./run_from_memory_cin.cpp 
foo
C: bar 42
C: GNU/Linux
bar
pi@raspberrypi400:~/memrun/C/mount_tmpfs $ 
```

Finally [mount_tmpfs/info.c](mount_tmpfs/info.c) is another example of shebang processing (#!/usr/local/bin/gcc -run). On shown invocation 31483 is process ID of running compiled info.c executable doit, 31496 is process ID of /usr/local/bin/grun, pointed to by gcc. The first directory printed shows mounted tmpfs containing executable doit. During compilation temporary gcc files were stored in that directory as well. File descriptor 3 in second directory is the loop device needed for mounting tmpfs under "/proc/31483/fd":
```
pi@raspberrypi400:~/memrun/C/mount_tmpfs $ ./info.c 123
My process ID : 31496
argv[0] : /proc/31483/fd/doit
argv[1] : 123

evecve --> /usr/bin/ls -al /proc/31496/fd/ /proc/31483/fd
/proc/31483/fd:
total 8
drwxrwxrwt 2 pi pi   60 Oct  7 13:23 .
dr-xr-xr-x 9 pi pi    0 Oct  7 13:23 ..
-rwxr-xr-x 1 pi pi 8112 Oct  7 13:23 doit

/proc/31496/fd/:
total 0
dr-x------ 2 pi pi  0 Oct  7 13:23 .
dr-xr-xr-x 9 pi pi  0 Oct  7 13:23 ..
lrwx------ 1 pi pi 64 Oct  7 13:23 0 -> /dev/pts/0
lrwx------ 1 pi pi 64 Oct  7 13:23 1 -> /dev/pts/0
lrwx------ 1 pi pi 64 Oct  7 13:23 2 -> /dev/pts/0
lr-x------ 1 pi pi 64 Oct  7 13:23 3 -> /proc/31496/fd
pi@raspberrypi400:~/memrun/C/mount_tmpfs $ 
```
