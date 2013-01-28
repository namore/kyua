dnl Copyright 2010 Google Inc.
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions are
dnl met:
dnl
dnl * Redistributions of source code must retain the above copyright
dnl   notice, this list of conditions and the following disclaimer.
dnl * Redistributions in binary form must reproduce the above copyright
dnl   notice, this list of conditions and the following disclaimer in the
dnl   documentation and/or other materials provided with the distribution.
dnl * Neither the name of Google Inc. nor the names of its contributors
dnl   may be used to endorse or promote products derived from this software
dnl   without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
dnl "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
dnl LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
dnl A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
dnl OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
dnl SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
dnl LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
dnl DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
dnl THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
dnl (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
dnl OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


dnl Checks if optreset exists to reset the processing of getopt(3) options.
dnl
dnl getopt(3) has an optreset global variable to reset internal state
dnl before calling getopt(3) again.  However, optreset is not standard and
dnl is only present in the BSD versions of getopt(3).
dnl
dnl Defines HAVE_GETOPT_WITH_OPTRESET if optreset exists.
AC_DEFUN([_KYUA_GETOPT_WITH_OPTRESET], [
    AC_MSG_CHECKING([whether getopt has optreset])

    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <stdlib.h>
#include <unistd.h>

int
main(void)
{
    optreset = 1;
    return EXIT_SUCCESS;
}
])],
    [getopt_optreset=yes
     AC_DEFINE([HAVE_GETOPT_WITH_OPTRESET], [1],
               [Define to 1 if getopt has optreset])],
    [getopt_optreset=no])

    AC_MSG_RESULT([${getopt_optreset}])
])


dnl Checks the value to pass to optind to reset getopt(3) processing.
dnl
dnl The standard value to pass to optind to reset the processing of command
dnl lines with getopt(3) is 1.  However, the GNU extensions to getopt_long(3)
dnl are not properly reset unless optind is set to 0, causing crashes later
dnl on and incorrect option processing behavior.
dnl
dnl Sets the GETOPT_OPTIND_RESET_VALUE macro to the integer value that has to
dnl be passed to optind to reset option processing.
AC_DEFUN([_KYUA_GETOPT_OPTIND_RESET_VALUE], [
    AC_MSG_CHECKING([for the optind value to reset getopt processing])

    AC_RUN_IFELSE([AC_LANG_SOURCE([
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
first_pass(void)
{
    int argc, ch, flag;
    char* argv@<:@5@:>@;

    argc = 4;
    argv@<:@0@:>@ = strdup("progname");
    argv@<:@1@:>@ = strdup("-a");
    argv@<:@2@:>@ = strdup("foo");
    argv@<:@3@:>@ = strdup("bar");
    argv@<:@4@:>@ = NULL;

    flag = 0;
    while ((ch = getopt(argc, argv, "+:a")) != -1) {
        switch (ch) {
        case 'a':
            flag = 1;
            break;
        default:
            break;
        }
    }
    if (!flag) {
        exit(EXIT_FAILURE);
    }
}

static void
second_pass(void)
{
    int argc, ch, flag;
    char* argv@<:@5@:>@;

    argc = 4;
    argv@<:@0@:>@ = strdup("progname");
    argv@<:@1@:>@ = strdup("-b");
    argv@<:@2@:>@ = strdup("foo");
    argv@<:@3@:>@ = strdup("bar");
    argv@<:@4@:>@ = NULL;

    flag = 0;
    while ((ch = getopt(argc, argv, "b")) != -1) {
        switch (ch) {
        case 'b':
            flag = 1;
            break;
        default:
            break;
        }
    }
    if (!flag) {
        exit(EXIT_FAILURE);
    }
}

int
main(void)
{
    /* We do two passes in two different functions to prevent the reuse of
     * variables and, specially, to force the use of two different argument
     * vectors. */
    first_pass();
    optind = 0;
    second_pass();
    return EXIT_SUCCESS;
}
])],
    [optind_reset_value=0],
    [optind_reset_value=1])

    AC_DEFINE_UNQUOTED([GETOPT_OPTIND_RESET_VALUE], [${optind_reset_value}],
        [Define to the optind value to reset getopt processing])
    AC_MSG_RESULT([${optind_reset_value}])
])


dnl Wrapper macro to detect all getopt(3) necessary features.
AC_DEFUN([KYUA_GETOPT], [
    _KYUA_GETOPT_OPTIND_RESET_VALUE
    _KYUA_GETOPT_WITH_OPTRESET
])