## Overview

The function **fcntl(2)** of the POSIX standard, among other things, allows to make local locks on parts of a file:

https://man7.org/linux/man-pages/man2/fcntl.2.html

However, it has one significant design issue: this lock disappears if, after its installation, an **open(2)** and **close(2)** are made on the same file.

The goal of this project is to implement functionality similar to the advisory lock of the POSIX standard, but which will solve this weakness.

## Features
Function **rl_open** implements functionality of POSIX's function **open**. Similarly **rl_close** (**close**), **rl_dup** and **rl_dup2** (**dup**, **dup2**), **rl_fork** (**fork**) and **rl_fcntl** (**fcntl**) are implemented in this library.
Examples of their usage are presented in `test/test.c`.

### Compilation

` make`

### Usage
The files `/bin/librl_lock_library.a`  and `include/rl_lock_library.h` are available to use Region_lock_library in your projects.  [How to do it](https://www.cs.swarthmore.edu/~newhall/unixhelp/howto_C_libraries.html).

### Run tests
`./bin/rl_lock_test       <some_file_on_root_progect>          <index_test>`

<index_test> in range [0 .. 4]

<index_test> = 0 to run all tests.


## Maintainers
- BRAGINA Natalia
- VIGNESWARAN Tharsiya
