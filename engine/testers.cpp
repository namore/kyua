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

#include "engine/testers.hpp"

extern "C" {
#include <dirent.h>
#include <regex.h>
}

#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

#include "engine/exceptions.hpp"
#include "model/test_result.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/releaser.hpp"
#include "utils/stream.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;
namespace passwd = utils::passwd;
namespace process = utils::process;

using utils::none;
using utils::optional;


namespace {


/// Mapping of interface names to tester binaries.
typedef std::map< std::string, std::string > testers_map;


/// Collection of known-good interface to tester mappings.
static testers_map interfaces_to_testers;


/// Drops the trailing newline in a string and replaces others with a literal.
///
/// \param input The string in which to perform the replacements.
///
/// \return The modified string.
static std::string
replace_newlines(const std::string input)
{
    std::string output = input;

    while (output.length() > 0 && output[output.length() - 1] == '\n') {
        output.erase(output.end() - 1);
    }

    std::string::size_type newline = output.find('\n', 0);
    while (newline != std::string::npos) {
        output.replace(newline, 1, "<<NEWLINE>>");
        newline = output.find('\n', newline + 1);
    }

    return output;
}


/// Finds all available testers and caches their data.
///
/// \param [out] testers Map into which to store the list of available testers.
static void
load_testers(testers_map& testers)
{
    PRE(testers.empty());

    const fs::path raw_testersdir(utils::getenv_with_default(
        "KYUA_TESTERSDIR", KYUA_TESTERSDIR));
    const fs::path testersdir = raw_testersdir.is_absolute() ?
        raw_testersdir : raw_testersdir.to_absolute();

    ::DIR* dir = ::opendir(testersdir.c_str());
    if (dir == NULL) {
        const int original_errno = errno;
        LW(F("Failed to open testers dir %s: %s") % testersdir %
           strerror(original_errno));
        return;  // No testers available in the given location.
    }
    const utils::releaser< ::DIR, int > dir_releaser(dir, ::closedir);

    ::regex_t preg;
    if (::regcomp(&preg, "^kyua-(.+)-tester$", REG_EXTENDED) != 0)
        throw engine::error("Failed to compile regular expression");
    const utils::releaser< ::regex_t, void > preg_releaser(&preg, ::regfree);

    ::dirent* de;
    while ((de = readdir(dir)) != NULL) {
        ::regmatch_t matches[2];
        const int ret = ::regexec(&preg, de->d_name, 2, matches, 0);
        if (ret == 0) {
            const std::string interface(de->d_name + matches[1].rm_so,
                                        matches[1].rm_eo - matches[1].rm_so);
            const fs::path path = testersdir / de->d_name;
            LI(F("Found tester for interface %s in %s") % interface % path);
            INV(path.is_absolute());
            testers[interface] = path.str();
        } else if (ret == REG_NOMATCH) {
            // Not a tester; skip.
        } else {
            throw engine::error("Failed to match regular expression");
        }
    }
}


}  // anonymous namespace


/// Returns the name of all supported test interfaces.
///
/// \return Collection of test interface names.
std::set< std::string >
engine::all_test_interfaces(void)
{
    if (interfaces_to_testers.empty())
        load_testers(interfaces_to_testers);

    std::set< std::string > test_interfaces;
    for (testers_map::const_iterator iter = interfaces_to_testers.begin();
         iter != interfaces_to_testers.end(); ++iter)
        test_interfaces.insert((*iter).first);
    return test_interfaces;
}


/// Returns the path to a tester binary.
///
/// \param interface Name of the interface of the tester being looked for.
///
/// \return Absolute path to the tester.
fs::path
engine::tester_path(const std::string& interface)
{
    if (interfaces_to_testers.empty())
        load_testers(interfaces_to_testers);

    const testers_map::const_iterator iter = interfaces_to_testers.find(
        interface);
    if (iter == interfaces_to_testers.end())
        throw engine::error("Unknown interface " + interface);

    const fs::path path((*iter).second);
    INV(path.is_absolute());
    return path;
}


/// Constructs a tester.
///
/// \param interface Name of the interface to use.
/// \param unprivileged_user If not none, the user to switch to when running
///     the tester.
/// \param timeout If not none, the timeout to pass to the tester.
/// \param vars Collection of configuration variables.
engine::tester::tester(const std::string& interface,
                       const optional< passwd::user >& unprivileged_user,
                       const optional< datetime::delta >& timeout,
                       const std::map< std::string, std::string >& vars) :
    _interface(interface)
{
    if (unprivileged_user) {
        const passwd::user current_user = passwd::current_user();
        if (current_user.is_root()) {
            _common_args.push_back(F("-u%s") % unprivileged_user.get().uid);
            _common_args.push_back(F("-g%s") % unprivileged_user.get().gid);
        } else {
            LW(F("Ignoring value of unprivileged_user=%s; already running as "
                 "unprivileged user %s") %
               unprivileged_user.get().name % current_user.name);
        }
    }
    if (timeout) {
        PRE(timeout.get().useconds == 0);
        _common_args.push_back(F("-t%s") % timeout.get().seconds);
    }
    for (std::map< std::string, std::string >::const_iterator i = vars.begin();
         i != vars.end(); ++i) {
        _common_args.push_back(F("-v%s=%s") % (*i).first % (*i).second);
    }
}


/// Destructor.
engine::tester::~tester(void)
{
}


/// Executes a list operation on a test program.
///
/// \param program Path to the test program.
///
/// \return The output of the tester, which represents a valid list of test
/// cases.
///
/// \throw error If the tester returns with an unsuccessful exit code.
std::string
engine::tester::list(const fs::path& program) const
{
    std::vector< std::string > args = _common_args;
    args.push_back("list");
    args.push_back(program.str());

    const fs::path tester_path = engine::tester_path(_interface);
    std::auto_ptr< process::child > child = process::child::spawn_capture(
        tester_path, args);

    const std::string output = utils::read_stream(child->output());

    const process::status status = child->wait();
    if (!status.exited() || status.exitstatus() != EXIT_SUCCESS)
        throw engine::error("Tester did not exit cleanly: " +
                            replace_newlines(output));
    return output;
}


/// Executes a test operation on a test case.
///
/// \param program Path to the test program.
/// \param test_case_name Name of the test case to execute.
/// \param result_file Path to the file in which to leave the result of the
///     tester invocation.
/// \param stdout_file Path to the file in which to store the stdout.
/// \param stderr_file Path to the file in which to store the stderr.
///
/// \throw error If the tester returns with an unsuccessful exit code.
void
engine::tester::test(const fs::path& program, const std::string& test_case_name,
                     const fs::path& result_file, const fs::path& stdout_file,
                     const fs::path& stderr_file) const
{
    std::vector< std::string > args = _common_args;
    args.push_back("test");
    args.push_back(program.str());
    args.push_back(test_case_name);
    args.push_back(result_file.str());

    const fs::path tester_path = engine::tester_path(_interface);
    std::auto_ptr< process::child > child = process::child::spawn_files(
        tester_path, args, stdout_file, stderr_file);
    const process::status status = child->wait();

    if (status.exited()) {
        if (status.exitstatus() == EXIT_SUCCESS) {
            // OK; the tester exited cleanly.
        } else if (status.exitstatus() == EXIT_FAILURE) {
            // OK; the tester reported that the test itself failed and we have
            // the result file to indicate this.
        } else {
            throw engine::error(F("Tester failed with code %s; this is a bug") %
                                status.exitstatus());
        }
    } else {
        INV(status.signaled());
        throw engine::error(F("Tester received signal %s; this is a bug") %
                            status.termsig());
    }
}


/// Parses a result from an input stream.
///
/// The parsing of a results file is quite permissive in terms of file syntax
/// validation.  We accept result files with or without trailing new lines, and
/// with descriptions that may span multiple lines.  This is to avoid getting in
/// trouble when the result is generated from user code, in which case it is
/// hard to predict how newlines look like.  Just swallow them; it's better for
/// the consumer.
///
/// \param input The stream from which to read the result.
///
/// \return The parsed result.  If there is any problem during parsing, the
/// failure is encoded as a broken result.
model::test_result
engine::parse_test_result(std::istream& input)
{
    std::string line;
    if (!std::getline(input, line).good() && line.empty())
        return model::test_result(model::test_result_broken,
                                  "Empty result file");

    // Fast-path for the most common case.
    if (line == "passed")
        return model::test_result(model::test_result_passed);

    std::string type, reason;
    const std::string::size_type pos = line.find(": ");
    if (pos == std::string::npos) {
        type = line;
        reason = "";
    } else {
        type = line.substr(0, pos);
        reason = line.substr(pos + 2);
    }

    if (input.good()) {
        line.clear();
        while (std::getline(input, line).good() && !line.empty()) {
            reason += "<<NEWLINE>>" + line;
            line.clear();
        }
        if (!line.empty())
            reason += "<<NEWLINE>>" + line;
    }

    if (type == "broken") {
        return model::test_result(model::test_result_broken, reason);
    } else if (type == "expected_failure") {
        return model::test_result(model::test_result_expected_failure,
                                  reason);
    } else if (type == "failed") {
        return model::test_result(model::test_result_failed, reason);
    } else if (type == "passed") {
        return model::test_result(model::test_result_passed, reason);
    } else if (type == "skipped") {
        return model::test_result(model::test_result_skipped, reason);
    } else {
        return model::test_result(model::test_result_broken,
                                  F("Unknown result type '%s'") % type);
    }
}
