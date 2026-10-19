# Release tags are the only source of the active Qt application's version.
find_package(Git REQUIRED)
if(NOT DEFINED LIGHTER_SOURCE_DIR)
  get_filename_component(LIGHTER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
set(_version_tag_regex "^v(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
if(NOT EXISTS "${LIGHTER_SOURCE_DIR}/.git")
  message(FATAL_ERROR "Build from a Git checkout with version tags; run git clone and git fetch --tags.")
endif()

execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" RESULT_VARIABLE _git_result
  OUTPUT_VARIABLE LIGHTER_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT _git_result EQUAL 0)
  message(FATAL_ERROR "Build from a Git checkout with version tags; run git clone and git fetch --tags.")
endif()
string(SUBSTRING "${LIGHTER_GIT_REVISION}" 0 7 LIGHTER_GIT_COMMIT)

if("$ENV{GITHUB_REF_TYPE}" STREQUAL "tag")
  set(LIGHTER_VERSION_TAG "$ENV{GITHUB_REF_NAME}")
  if(NOT LIGHTER_VERSION_TAG MATCHES "${_version_tag_regex}")
    message(FATAL_ERROR "Release tag must have the form vMAJOR.MINOR.PATCH: ${LIGHTER_VERSION_TAG}")
  endif()
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse "refs/tags/${LIGHTER_VERSION_TAG}^{commit}"
    WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" RESULT_VARIABLE _tag_result
    OUTPUT_VARIABLE _tag_revision OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(NOT _tag_result EQUAL 0 OR NOT _tag_revision STREQUAL LIGHTER_GIT_REVISION)
    message(FATAL_ERROR "Release tag ${LIGHTER_VERSION_TAG} does not point to the checked-out commit.")
  endif()
else()
  execute_process(COMMAND "${GIT_EXECUTABLE}" tag --list --merged HEAD
    WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" OUTPUT_VARIABLE _tags
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  string(REPLACE "\n" ";" _tags "${_tags}")
  set(_tag_matches)
  foreach(_tag IN LISTS _tags)
    if(_tag MATCHES "${_version_tag_regex}")
      list(APPEND _tag_matches "--match=${_tag}")
    endif()
  endforeach()
  if(NOT _tag_matches)
    message(FATAL_ERROR "No reachable vMAJOR.MINOR.PATCH tag found. Fetch full history and tags: git fetch --unshallow --tags (shallow clone) or git fetch --tags.")
  endif()
  execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 ${_tag_matches} HEAD
    WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" OUTPUT_VARIABLE LIGHTER_VERSION_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
endif()

string(SUBSTRING "${LIGHTER_VERSION_TAG}" 1 -1 LIGHTER_VERSION)
message(STATUS "Lighter ${LIGHTER_VERSION} from ${LIGHTER_VERSION_TAG} (${LIGHTER_GIT_COMMIT})")
if(DEFINED LIGHTER_VERSION_OUTPUT)
  file(WRITE "${LIGHTER_VERSION_OUTPUT}"
    "{\n  \"version\": \"${LIGHTER_VERSION}\",\n  \"tag\": \"${LIGHTER_VERSION_TAG}\",\n  \"commit\": \"${LIGHTER_GIT_REVISION}\"\n}\n")
endif()

# Reconfigure existing build trees when HEAD or the set of tags changes.
if(NOT CMAKE_SCRIPT_MODE_FILE)
  execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
    WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" OUTPUT_VARIABLE _head_ref OUTPUT_STRIP_TRAILING_WHITESPACE)
  foreach(_ref IN ITEMS HEAD packed-refs "${_head_ref}")
    if(_ref STREQUAL "")
      continue()
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${_ref}"
      WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" OUTPUT_VARIABLE _ref_file OUTPUT_STRIP_TRAILING_WHITESPACE)
    get_filename_component(_ref_file "${_ref_file}" ABSOLUTE BASE_DIR "${LIGHTER_SOURCE_DIR}")
    if(EXISTS "${_ref_file}")
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ref_file}")
    endif()
  endforeach()
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path refs/tags
    WORKING_DIRECTORY "${LIGHTER_SOURCE_DIR}" OUTPUT_VARIABLE _tag_dir OUTPUT_STRIP_TRAILING_WHITESPACE)
  get_filename_component(_tag_dir "${_tag_dir}" ABSOLUTE BASE_DIR "${LIGHTER_SOURCE_DIR}")
  file(GLOB_RECURSE _tag_refs CONFIGURE_DEPENDS "${_tag_dir}/*")
  if(_tag_refs)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${_tag_refs})
  endif()
endif()
