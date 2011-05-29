# Copyright 2011 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Location of installed documents.  Used to validate the output of the about
# messages against the golden files.
KYUA_DOCDIR='__KYUA_DOCDIR__'


# Common code to validate the output of all about information.
#
# \param file The name of the file with the output.
check_all() {
    local file="${1}"; shift

    grep -E 'kyua-cli.*[0-9]+\.[0-9]+' "${file}" || \
        atf_fail 'No version reported'
    grep 'Copyright' "${file}" || atf_fail 'No license reported'
    grep '<.*@.*>' "${file}" || atf_fail 'No authors reported'
    grep 'Homepage' "${file}" || atf_fail 'No homepage reported'
}


# Checks if an installed file is available and, if not, skips the test.
#
# \param file The name of the file to check for.
#
# \todo This needs to become "require.files" in the metadata.
require_file() {
    local file="${1}"; shift

    test -f "${file}" || atf_skip "File ${file} not available; maybe it is" \
        "not installed yet?"
}


utils_test_case default__installed
default__installed_body() {
    require_file "${KYUA_DOCDIR}/AUTHORS"
    require_file "${KYUA_DOCDIR}/COPYING"
    atf_check -s exit:0 -o save:stdout -e empty kyua about
    check_all stdout
}


utils_test_case default__override
default__override_body() {
    mkdir docs
    echo "Author <author@example.net>" >docs/AUTHORS
    echo "Copyright text" >docs/COPYING
    export KYUA_DOCDIR=docs
    atf_check -s exit:0 -o save:stdout -e empty kyua about
    check_all stdout
}


utils_test_case show__all__installed
show__all__installed_body() {
    require_file "${KYUA_DOCDIR}/AUTHORS"
    require_file "${KYUA_DOCDIR}/COPYING"
    atf_check -s exit:0 -o save:stdout -e empty kyua about --show=all
    check_all stdout
}


utils_test_case show__all__override
show__all__override_body() {
    mkdir docs
    echo "Author <author@example.net>" >docs/AUTHORS
    echo "Copyright text" >docs/COPYING
    export KYUA_DOCDIR=docs
    atf_check -s exit:0 -o save:stdout -e empty kyua about --show=all
    check_all stdout
}


utils_test_case show__authors__installed
show__authors__installed_body() {
    require_file "${KYUA_DOCDIR}/AUTHORS"
    atf_check -s exit:0 -o file:"${KYUA_DOCDIR}/AUTHORS" -e empty \
        kyua about --show=authors
}


utils_test_case show__authors__override
show__authors__override_body() {
    mkdir docs
    echo "Author <author@example.net>" >docs/AUTHORS
    export KYUA_DOCDIR=docs
    atf_check -s exit:0 -o file:docs/AUTHORS -e empty \
        kyua about --show=authors
}


utils_test_case show__license__installed
show__license__installed_body() {
    require_file "${KYUA_DOCDIR}/COPYING"
    atf_check -s exit:0 -o file:"${KYUA_DOCDIR}/COPYING" -e empty \
        kyua about --show=license
}


utils_test_case show__license__override
show__license__override_body() {
    mkdir docs
    echo "Copyright text" >docs/COPYING
    export KYUA_DOCDIR=docs
    atf_check -s exit:0 -o file:docs/COPYING -e empty \
        kyua about --show=license
}


utils_test_case show__version
show__version_body() {
    atf_check -s exit:0 -o save:stdout -e empty kyua about --show=version

    local lines="$(wc -l stdout | awk '{ print $1 }')"
    [ "${lines}" -eq 1 ] || atf_fail "Version query returned more than one line"

    grep '^kyua-cli (.*) [0-9]\.[0-9]$' stdout || \
        atf_fail "Invalid version message"
}


utils_test_case show__invalid
show__invalid_body() {
    cat >experr <<EOF
Usage error for command about: Invalid value passed to --show: foo.
Type 'kyua help about' for usage information.
EOF
    atf_check -s exit:1 -o empty -e file:experr kyua about --show=foo
}


utils_test_case too_many_arguments
too_many_arguments_body() {
    cat >stderr <<EOF
Usage error for command about: Too many arguments.
Type 'kyua help about' for usage information.
EOF
    atf_check -s exit:1 -o empty -e file:stderr kyua about abc
}


atf_init_test_cases() {
    atf_add_test_case default__installed
    atf_add_test_case default__override

    atf_add_test_case show__all__installed
    atf_add_test_case show__all__override
    atf_add_test_case show__authors__installed
    atf_add_test_case show__authors__override
    atf_add_test_case show__license__installed
    atf_add_test_case show__license__override
    atf_add_test_case show__version
    atf_add_test_case show__invalid

    atf_add_test_case too_many_arguments
}
