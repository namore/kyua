// Copyright 2012 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include <atf-c.h>

#define INTERFACE "atf"
#include "testers/common_inttest.h"
#include "testers/error.h"
#include "testers/fs.h"
#include "testers/text.h"


static const char* test_cases_list =
    "test_case{name='fail'}\n"
    "test_case{name='pass'}\n"
    "test_case{name='signal'}\n"
    "test_case{name='sleep'}\n"
    "test_case{name='cleanup_check_work_directory', has_cleanup='true'}\n"
    "test_case{name='cleanup_fail', has_cleanup='true'}\n"
    "test_case{name='cleanup_signal', has_cleanup='true'}\n"
    "test_case{name='cleanup_sleep', has_cleanup='true'}\n"
    "test_case{name='body_and_cleanup_fail', has_cleanup='true'}\n"
    "test_case{name='print_config', has_cleanup='true'}\n";


ATF_TC(list__ok);
ATF_TC_HEAD(list__ok, tc) { setup(tc, true); }
ATF_TC_BODY(list__ok, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_SUCCESS, test_cases_list, "", "list", helpers, NULL);
    free(helpers);
}


ATF_TC(list__config);
ATF_TC_HEAD(list__config, tc) { setup(tc, true); }
ATF_TC_BODY(list__config, tc)
{
    char* current_dir;
    ATF_REQUIRE(!kyua_error_is_set(kyua_fs_current_path(&current_dir)));
    char* vflag;
    ATF_REQUIRE(!kyua_error_is_set(kyua_text_printf(
        &vflag, "-vconfig_in_list_cookie=%s/cookie.txt", current_dir)));
    free(current_dir);

    char* helpers = helpers_path(tc);
    ATF_REQUIRE(!atf_utils_file_exists("cookie.txt"));
    check(EXIT_SUCCESS, test_cases_list, "", vflag, "list", helpers, NULL);
    ATF_REQUIRE( atf_utils_file_exists("cookie.txt"));
    free(helpers);

    free(vflag);
}


ATF_TC(test__pass);
ATF_TC_HEAD(test__pass, tc) { setup(tc, true); }
ATF_TC_BODY(test__pass, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_SUCCESS,
          "First line to stdout\nSecond line to stdout\n",
          "First line to stderr\nSecond line to stderr\n",
          "test", helpers, "pass", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "passed\n"));
}


ATF_TC(test__fail);
ATF_TC_HEAD(test__fail, tc) { setup(tc, true); }
ATF_TC_BODY(test__fail, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "First line to stdout\n", "First line to stderr\n",
          "test", helpers, "fail", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
        "failed: This is the failure message\n"));
}


ATF_TC(test__crash);
ATF_TC_HEAD(test__crash, tc) { setup(tc, true); }
ATF_TC_BODY(test__crash, tc)
{
    require_coredump_ability();

    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "", "save:crash.err",
          "test", helpers, "signal", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
        "broken: Premature exit; test case received signal 6 (core dumped)\n"));

    ATF_REQUIRE(atf_utils_grep_file("About to die due to SIGABRT!",
                                    "crash.err"));
    ATF_REQUIRE(atf_utils_grep_file("attempting to gather stack trace",
                                    "crash.err"));
}


ATF_TC(test__timeout);
ATF_TC_HEAD(test__timeout, tc) { setup(tc, true); }
ATF_TC_BODY(test__timeout, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "", "Subprocess timed out; sending KILL signal...\n",
          "-t1", "test", helpers, "sleep", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
                                       "broken: Test case body timed out\n"));
}


ATF_TC(test__result_priority);
ATF_TC_HEAD(test__result_priority, tc) { setup(tc, true); }
ATF_TC_BODY(test__result_priority, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "Killing cleanup\n", "",
          "-vhas.cleanup=true", "test", helpers, "body_and_cleanup_fail",
          "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "failed: Body fails\n"));
}


ATF_TC(test__cleanup__ok);
ATF_TC_HEAD(test__cleanup__ok, tc) { setup(tc, true); }
ATF_TC_BODY(test__cleanup__ok, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_SUCCESS,
          "Body stdout\nCleanup stdout\n"
              "Cleanup properly ran in the same directory as the body\n",
          "Body stderr\nCleanup stderr\n",
          "-vhas.cleanup=true", "test", helpers, "cleanup_check_work_directory",
          "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "passed\n"));
}


ATF_TC(test__cleanup__fail);
ATF_TC_HEAD(test__cleanup__fail, tc) { setup(tc, true); }
ATF_TC_BODY(test__cleanup__fail, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "", "",
          "-vhas.cleanup=true", "test", helpers, "cleanup_fail", "test-result",
          NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "broken: Test case "
        "cleanup exited with code 1\n"));
}


ATF_TC(test__cleanup__crash);
ATF_TC_HEAD(test__cleanup__crash, tc) { setup(tc, true); }
ATF_TC_BODY(test__cleanup__crash, tc)
{
    require_coredump_ability();

    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "", "save:crash.err",
          "-vhas.cleanup=true", "test", helpers, "cleanup_signal",
          "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
        "broken: Test case cleanup received signal 6 (core dumped)\n"));

    ATF_REQUIRE(atf_utils_grep_file("About to die due to SIGABRT!",
                                    "crash.err"));
    ATF_REQUIRE(atf_utils_grep_file("attempting to gather stack trace",
                                    "crash.err"));
}


ATF_TC(test__cleanup__timeout);
ATF_TC_HEAD(test__cleanup__timeout, tc) { setup(tc, true); }
ATF_TC_BODY(test__cleanup__timeout, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "", "Subprocess timed out; sending KILL signal...\n",
          "-t1", "-vhas.cleanup=true", "test", helpers, "cleanup_sleep",
          "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "broken: Test case "
        "cleanup timed out\n"));
}


ATF_TC(test__config__builtin);
ATF_TC_HEAD(test__config__builtin, tc) { setup(tc, true); }
ATF_TC_BODY(test__config__builtin, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_SUCCESS, "", "",
          "test", helpers, "print_config", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "passed\n"));
}


ATF_TC(test__config__custom);
ATF_TC_HEAD(test__config__custom, tc) { setup(tc, true); }
ATF_TC_BODY(test__config__custom, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_SUCCESS,
          "body my-var1 value1\n"
          "body v2 a b c foo\n"
          "cleanup my-var1 value1\n"
          "cleanup v2 a b c foo\n",
          "",
          "-vmy-var1=value1", "-vv2=a b c foo", "test", helpers,
          "print_config", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result", "passed\n"));
}


ATF_TC(test__invalid_test_case_name);
ATF_TC_HEAD(test__invalid_test_case_name, tc) { setup(tc, false); }
ATF_TC_BODY(test__invalid_test_case_name, tc)
{
    char* helpers = helpers_path(tc);
    check(EXIT_FAILURE, "",  // TODO(jmmv): Should be EXIT_INTERNAL_ERROR.
          "atf_helpers: ERROR: Unknown test case `foo'\n"
          "atf_helpers: See atf-test-program(1) for usage details.\n",
          "-vhas.cleanup=false", "test", helpers, "foo", "test-result", NULL);
    free(helpers);

    ATF_REQUIRE(atf_utils_compare_file("test-result",
        "broken: Premature exit; test case exited with code 1\n"));
}


ATF_TC(test__missing_test_program);
ATF_TC_HEAD(test__missing_test_program, tc) { setup(tc, false); }
ATF_TC_BODY(test__missing_test_program, tc)
{
    check(EXIT_INTERNAL_ERROR, "",
          "kyua-atf-tester: execvp failed: No such file or directory\n",
          "test", "./non-existent", "pass", "test-result", NULL);

    ATF_REQUIRE(!atf_utils_file_exists("test-result"));
}


ATF_TP_ADD_TCS(tp)
{
    ATF_TP_ADD_TC(tp, top__missing_command);
    ATF_TP_ADD_TC(tp, top__unknown_command);

    ATF_TP_ADD_TC(tp, list__ok);
    ATF_TP_ADD_TC(tp, list__config);

    ATF_TP_ADD_TC(tp, test__pass);
    ATF_TP_ADD_TC(tp, test__fail);
    ATF_TP_ADD_TC(tp, test__crash);
    ATF_TP_ADD_TC(tp, test__timeout);
    ATF_TP_ADD_TC(tp, test__result_priority);
    ATF_TP_ADD_TC(tp, test__cleanup__ok);
    ATF_TP_ADD_TC(tp, test__cleanup__fail);
    ATF_TP_ADD_TC(tp, test__cleanup__crash);
    ATF_TP_ADD_TC(tp, test__cleanup__timeout);
    ATF_TP_ADD_TC(tp, test__config__builtin);
    ATF_TP_ADD_TC(tp, test__config__custom);
    ATF_TP_ADD_TC(tp, test__missing_test_program);
    ATF_TP_ADD_TC(tp, test__invalid_test_case_name);

    return atf_no_error();
}
