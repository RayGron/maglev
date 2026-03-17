set(_maglev_vcpkg_candidates "")

get_filename_component(_maglev_source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT WIN32)
  set(_maglev_tools_bin "${_maglev_source_dir}/tools/bin")
  if(EXISTS "${_maglev_tools_bin}")
    if(DEFINED ENV{PATH} AND NOT "$ENV{PATH}" STREQUAL "")
      set(ENV{PATH} "${_maglev_tools_bin}:$ENV{PATH}")
    else()
      set(ENV{PATH} "${_maglev_tools_bin}")
    endif()

    if(EXISTS "${_maglev_tools_bin}/pkg-config")
      set(ENV{PKG_CONFIG} "${_maglev_tools_bin}/pkg-config")
    endif()
  endif()

  if(NOT DEFINED ENV{VCPKG_BINARY_SOURCES} OR "$ENV{VCPKG_BINARY_SOURCES}" STREQUAL "")
    set(ENV{VCPKG_BINARY_SOURCES} "clear")
  endif()
endif()

if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
  list(APPEND _maglev_vcpkg_candidates "$ENV{VCPKG_ROOT}")
endif()

list(
  APPEND
  _maglev_vcpkg_candidates
  "${_maglev_source_dir}/vcpkg"
  "${_maglev_source_dir}/.vcpkg"
  "${_maglev_source_dir}/../vcpkg"
  "${_maglev_source_dir}/../.vcpkg"
)

if(WIN32)
  execute_process(
    COMMAND where vcpkg.exe
    OUTPUT_VARIABLE _maglev_vcpkg_where
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(_maglev_vcpkg_where)
    string(REPLACE "\r\n" ";" _maglev_vcpkg_bins "${_maglev_vcpkg_where}")
    string(REPLACE "\n" ";" _maglev_vcpkg_bins "${_maglev_vcpkg_bins}")
    foreach(_maglev_vcpkg_bin IN LISTS _maglev_vcpkg_bins)
      get_filename_component(_maglev_vcpkg_bin_dir "${_maglev_vcpkg_bin}" DIRECTORY)
      list(APPEND _maglev_vcpkg_candidates "${_maglev_vcpkg_bin_dir}")
      get_filename_component(_maglev_vcpkg_root_parent "${_maglev_vcpkg_bin_dir}" DIRECTORY)
      list(APPEND _maglev_vcpkg_candidates "${_maglev_vcpkg_root_parent}")
    endforeach()
  endif()
else()
  find_program(_maglev_vcpkg_bin vcpkg)
  if(_maglev_vcpkg_bin)
    get_filename_component(_maglev_vcpkg_bin_dir "${_maglev_vcpkg_bin}" DIRECTORY)
    list(APPEND _maglev_vcpkg_candidates "${_maglev_vcpkg_bin_dir}")
    get_filename_component(_maglev_vcpkg_root_parent "${_maglev_vcpkg_bin_dir}" DIRECTORY)
    list(APPEND _maglev_vcpkg_candidates "${_maglev_vcpkg_root_parent}")

    if(EXISTS "${_maglev_vcpkg_bin}")
      file(READ "${_maglev_vcpkg_bin}" _maglev_vcpkg_wrapper_contents LIMIT 4096)
      string(
        REGEX MATCH
        "(/[^ \n\r\t\"']+/vcpkg(\\.exe)?)"
        _maglev_vcpkg_wrapper_target
        "${_maglev_vcpkg_wrapper_contents}"
      )
      if(_maglev_vcpkg_wrapper_target)
        get_filename_component(_maglev_vcpkg_wrapper_bin_dir "${_maglev_vcpkg_wrapper_target}" DIRECTORY)
        list(APPEND _maglev_vcpkg_candidates "${_maglev_vcpkg_wrapper_bin_dir}")
        get_filename_component(_maglev_vcpkg_wrapper_root_parent "${_maglev_vcpkg_wrapper_bin_dir}" DIRECTORY)
        list(APPEND _maglev_vcpkg_candidates "${_maglev_vcpkg_wrapper_root_parent}")
      endif()
    endif()
  endif()
endif()

list(REMOVE_DUPLICATES _maglev_vcpkg_candidates)

set(_maglev_vcpkg_found FALSE)
foreach(_maglev_candidate IN LISTS _maglev_vcpkg_candidates)
  if(EXISTS "${_maglev_candidate}/scripts/buildsystems/vcpkg.cmake")
    set(VCPKG_ROOT "${_maglev_candidate}" CACHE PATH "Resolved vcpkg root")
    set(ENV{VCPKG_ROOT} "${_maglev_candidate}")
    include("${_maglev_candidate}/scripts/buildsystems/vcpkg.cmake")
    set(_maglev_vcpkg_found TRUE)
    break()
  endif()
endforeach()

if(NOT _maglev_vcpkg_found)
  message(
    FATAL_ERROR
      "Could not locate vcpkg. Set VCPKG_ROOT, add vcpkg to PATH, or place a vcpkg checkout next to the repository."
  )
endif()
