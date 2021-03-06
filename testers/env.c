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

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

#include "testers/env.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testers/error.h"
#include "testers/text.h"


/// Sets an environment variable.
///
/// \param name Name of the environment variable to set.
/// \param value Value to be set.
///
/// \return An error object.
kyua_error_t
kyua_env_set(const char* name, const char* value)
{
    kyua_error_t error;

#if defined(HAVE_SETENV)
    if (setenv(name, value, 1) == -1)
        error = kyua_libc_error_new(
            errno, "Failed to set environment variable %s to %s", name, value);
    else
        error = kyua_error_ok();
#elif defined(HAVE_PUTENV)
    const size_t length = strlen(name) + strlen(value) + 2;
    char* buffer = (char*)malloc(length);
    if (buffer == NULL)
        error = kyua_oom_error_new();
    else {
        const size_t printed_length = snprintf(buffer, length, "%s=%s", name,
                                               value);
        assert(length == printed_length + 1);
        if (putenv(buffer) == -1) {
            error = kyua_libc_error_new(
                errno, "Failed to set environment variable %s to %s",
                name, value);
            free(buffer);
        } else
            error = kyua_error_ok();
    }
#else
#   error "Don't know how to set an environment variable."
#endif

    return error;
}


/// Unsets an environment variable.
///
/// \param name Name of the environment variable to unset.
///
/// \return An error object.
kyua_error_t
kyua_env_unset(const char* name)
{
#if defined(HAVE_UNSETENV)
    if (unsetenv(name) == -1)
        return kyua_libc_error_new(
            errno, "Failed to unset environment variable %s", name);
    else
        return kyua_error_ok();
#elif defined(HAVE_PUTENV)
    return kyua_env_set(name, "");
#else
#   error "Don't know how to unset an environment variable."
#endif
}


/// Validates that the configuration variables can be set in the environment.
///
/// \param user_variables Set of configuration variables to pass to the test.
///     This is an array of strings of the form var=value.
///
/// \return An error if there is a syntax error in the variables.
kyua_error_t
kyua_env_check_configuration(const char* const user_variables[])
{
    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter) {
        const char* var_value = *iter;
        if (strlen(var_value) == 0 ||
            (var_value)[0] == '=' ||
            strchr(var_value, '=') == NULL) {
            return kyua_generic_error_new("Invalid variable '%s'; must be of "
                                          "the form var=value", var_value);
        }
    }
    return kyua_error_ok();
}


/// Sets a collection of configuration variables in the environment.
///
/// \param user_variables Set of configuration variables to pass to the test.
///     This is an array of strings of the form var=value and must have been
///     previously sanity-checked with kyua_env_check_configuration.
///
/// \return An error if there is a problem allocating memory.
///
/// \post The environment contains a new collection of TEST_ENV_* variables that
/// matches the input user_variables.
kyua_error_t
kyua_env_set_configuration(const char* const user_variables[])
{
    const char* const* iter;
    for (iter = user_variables; *iter != NULL; ++iter) {
        kyua_error_t error;

        char* var_value = strdup(*iter);
        if (var_value == NULL)
            return kyua_oom_error_new();

        char* value = strchr(var_value, '=');
        assert(value != NULL);  // Must have been validated.
        *value = '\0';
        value += 1;

        char* var;
        error = kyua_text_printf(&var, "TEST_ENV_%s", var_value);
        if (kyua_error_is_set(error)) {
            free(var_value);
            return error;
        }

        error = kyua_env_set(var, value);
        if (kyua_error_is_set(error)) {
            free(var_value);
            return error;
        }
    }
    return kyua_error_ok();
}
