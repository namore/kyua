// Copyright 2011 Google Inc.
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

#include "drivers/debug_test.hpp"

#include <stdexcept>
#include <utility>

#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "engine/runner.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/auto_cleaners.hpp"
#include "utils/optional.ipp"
#include "utils/signals/interrupts.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace runner = engine::runner;
namespace signals = utils::signals;

using utils::none;
using utils::optional;


namespace {


/// Looks for a single test case in the Kyuafile.
///
/// \param filter A filter to match the desired test case.
/// \param [in,out] test_programs The test programs in the test suite.
///     Updated with the list of loaded test cases.
///
/// \return A pointer to the test case if found.
///
/// \throw std::runtime_error If the provided filter matches more than one test
///     case or if the test case cannot be found.
static std::pair< model::test_program_ptr, std::string >
find_test_case(const engine::test_filter& filter,
               model::test_programs_vector& test_programs)
{
    std::pair< model::test_program_ptr, std::string > found;

    for (model::test_programs_vector::iterator p = test_programs.begin();
         p != test_programs.end(); p++) {
        model::test_program_ptr& test_program = *p;

        if (!filter.matches_test_program(test_program->relative_path()))
            continue;

        const model::test_cases_map test_cases = test_program->test_cases();

        for (model::test_cases_map::const_iterator
             iter = test_cases.begin(); iter != test_cases.end(); iter++) {
            const model::test_case& test_case = (*iter).second;

            if (filter.matches_test_case(test_program->relative_path(),
                                         test_case.name())) {
                if (found.first.get() != NULL)
                    throw std::runtime_error(F("The filter '%s' matches more "
                                               "than one test case") %
                                             filter.str());
                found = std::make_pair(test_program, test_case.name());;
            }
        }
    }

    if (found.first.get() == NULL)
        throw std::runtime_error(F("Unknown test case '%s'") % filter.str());

    return found;
}


}  // anonymous namespace


/// Executes the operation.
///
/// \param kyuafile_path The path to the Kyuafile to be loaded.
/// \param build_root If not none, path to the built test programs.
/// \param filter The test case filter to locate the test to debug.
/// \param user_config The end-user configuration properties.
/// \param stdout_path The name of the file into which to store the test case
///     stdout.
/// \param stderr_path The name of the file into which to store the test case
///     stderr.
///
/// \returns A structure with all results computed by this driver.
drivers::debug_test::result
drivers::debug_test::drive(const fs::path& kyuafile_path,
                           const optional< fs::path > build_root,
                           const engine::test_filter& filter,
                           const config::tree& user_config,
                           const fs::path& stdout_path,
                           const fs::path& stderr_path)
{
    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root, user_config);
    // TODO(jmmv): Copy the test programs so that we can lazily load their test
    // cases into them.  This is a hack and should be removed once we have a
    // nicer interface to running test cases.
    model::test_programs_vector test_programs = kyuafile.test_programs();
    const std::pair< model::test_program_ptr, std::string > found =
        find_test_case(filter, test_programs);
    const model::test_program_ptr test_program = found.first;
    const std::string& test_case_name = found.second;
    runner::test_case_hooks dummy_hooks;

    signals::interrupts_handler interrupts;

    const fs::auto_directory work_directory = fs::auto_directory::mkdtemp(
        "kyua.XXXXXX");

    const model::test_result test_result = runner::debug_test_case(
        test_program.get(), test_case_name, user_config, dummy_hooks,
        work_directory.directory(), stdout_path, stderr_path);

    signals::check_interrupt();
    return result(engine::test_filter(
        test_program->relative_path(), test_case_name), test_result);
}
