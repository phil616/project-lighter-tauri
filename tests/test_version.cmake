find_package(Git REQUIRED)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef _nonce)
set(_test_dir "${TEST_BINARY_DIR}/version-tests-${_nonce}")
set(_repo "${_test_dir}/repo")
file(MAKE_DIRECTORY "${_repo}")

function(_git)
  execute_process(COMMAND "${GIT_EXECUTABLE}" -c commit.gpgsign=false -c tag.gpgsign=false
    -c "core.hooksPath=${_test_dir}/no-hooks" -c user.name=VersionTest
    -c user.email=version-test@example.invalid ${ARGV}
    WORKING_DIRECTORY "${_repo}" OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
endfunction()

function(_check_version expected)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "GITHUB_REF_TYPE=${_ref_type}" "GITHUB_REF_NAME=${_ref_name}"
    "${CMAKE_COMMAND}" "-DLIGHTER_SOURCE_DIR=${_repo}"
    "-DLIGHTER_VERSION_OUTPUT=${_test_dir}/version.json" -P "${VERSION_MODULE}"
    RESULT_VARIABLE _result OUTPUT_VARIABLE _out ERROR_VARIABLE _error)
  if(expected STREQUAL "ERROR")
    if(_result EQUAL 0 OR NOT _error MATCHES "${ARGV1}")
      message(FATAL_ERROR "Expected version resolution to fail (${ARGV1}): ${_out}${_error}")
    endif()
  else()
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "Version resolution failed: ${_out}${_error}")
    endif()
    file(READ "${_test_dir}/version.json" _json)
    string(JSON _actual GET "${_json}" version)
    if(NOT _actual STREQUAL expected)
      message(FATAL_ERROR "Expected ${expected}, got ${_actual}")
    endif()
  endif()
endfunction()

_check_version(ERROR "Git checkout")
_git(init -q --initial-branch=main)
_git(commit -q --allow-empty -m initial)
_check_version(ERROR "No reachable")
_git(tag v2.0.2)
_git(tag v99.0.0-invalid)
_check_version(2.0.2)
_git(checkout -q -b future)
_git(commit -q --allow-empty -m future)
_git(tag v99.0.0)
_git(checkout -q main)
_git(commit -q --allow-empty -m next)
_check_version(2.0.2)
_git(tag -a v2.0.3 -m release)
_check_version(2.0.3)

set(_ref_type tag)
set(_ref_name v2.0.3)
_check_version(2.0.3)
set(_ref_name v2.0.2)
_check_version(ERROR "does not point")
set(_ref_name v2.0.3-invalid)
_check_version(ERROR "must have the form")
unset(_ref_type)
unset(_ref_name)
_git(commit -q --allow-empty -m after-release)
_check_version(2.0.3)

# A shallow checkout must fail instead of quietly using an unrelated version.
if(WIN32)
  set(_clone_url "file:///${_repo}")
else()
  set(_clone_url "file://${_repo}")
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" clone -q --depth 1 "${_clone_url}" "${_test_dir}/shallow"
  COMMAND_ERROR_IS_FATAL ANY)
set(_full_repo "${_repo}")
set(_repo "${_test_dir}/shallow")
_check_version(ERROR "No reachable")
set(_repo "${_full_repo}")

# New tags must invalidate an existing build tree without a manual reconfigure.
find_program(_ninja NAMES ninja ninja-build HINTS "${TEST_MAKE_BIN}")
if(_ninja)
  file(WRITE "${_repo}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.21)\nset(LIGHTER_SOURCE_DIR \"${_repo}\")\nset(LIGHTER_VERSION_OUTPUT \"\${CMAKE_CURRENT_BINARY_DIR}/version.json\")\ninclude(\"${VERSION_MODULE}\")\nproject(VersionFixture NONE)\n")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=GITHUB_REF_TYPE --unset=GITHUB_REF_NAME
    "${CMAKE_COMMAND}" -S "${_repo}" -B "${_test_dir}/build"
    -G Ninja "-DCMAKE_MAKE_PROGRAM=${_ninja}" OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
  _git(tag v2.0.4)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=GITHUB_REF_TYPE --unset=GITHUB_REF_NAME
    "${CMAKE_COMMAND}" --build "${_test_dir}/build"
    OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
  file(READ "${_test_dir}/build/version.json" _json)
  string(JSON _actual GET "${_json}" version)
  if(NOT _actual STREQUAL "2.0.4")
    message(FATAL_ERROR "New tag did not refresh the build-tree version: ${_actual}")
  endif()
endif()
message(STATUS "Tag version regression checks passed")
