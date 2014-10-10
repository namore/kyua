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

#include "drivers/list_tests.hpp"

#include "engine/exceptions.hpp"
#include "engine/filters.hpp"
#include "engine/kyuafile.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "utils/optional.ipp"

namespace config = utils::config;
namespace fs = utils::fs;

using utils::none;
using utils::optional;


namespace {


/// Lists a single test program.
///
/// \param program The test program to print.
/// \param filters [in,out] The filters used to select which test cases to
///     print.  These filters are updated on output to mark which of them
///     actually matched a test case.
/// \param hooks The runtime hooks.
static void
list_test_program(model::test_program& program,
                  engine::filters_state& filters,
                  drivers::list_tests::base_hooks& hooks)
{
    const model::test_cases_map test_cases = program.test_cases();

    for (model::test_cases_map::const_iterator iter = test_cases.begin();
         iter != test_cases.end(); iter++) {
        const std::string& test_case_name = (*iter).first;

        if (filters.match_test_case(program.relative_path(), test_case_name))
            hooks.got_test_case(program, test_case_name);
    }
}


}  // anonymous namespace


/// Pure abstract destructor.
drivers::list_tests::base_hooks::~base_hooks(void)
{
}


/// Executes the operation.
///
/// \param kyuafile_path The path to the Kyuafile to be loaded.
/// \param build_root If not none, path to the built test programs.
/// \param raw_filters The test case filters as provided by the user.
/// \param user_config The end-user configuration properties.
/// \param hooks The hooks for this execution.
///
/// \returns A structure with all results computed by this driver.
drivers::list_tests::result
drivers::list_tests::drive(const fs::path& kyuafile_path,
                           const optional< fs::path > build_root,
                           const std::set< engine::test_filter >& raw_filters,
                           const config::tree& user_config,
                           base_hooks& hooks)
{
    const engine::kyuafile kyuafile = engine::kyuafile::load(
        kyuafile_path, build_root, user_config);
    engine::filters_state filters(raw_filters);

    for (model::test_programs_vector::const_iterator iter =
         kyuafile.test_programs().begin();
         iter != kyuafile.test_programs().end(); iter++) {
        const model::test_program_ptr& test_program = *iter;

        if (!filters.match_test_program(test_program->relative_path()))
            continue;

        list_test_program(*test_program, filters, hooks);
    }

    return result(filters.unused());
}
