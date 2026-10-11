# Wraps CMake's FetchContent for the SDK's CMake 3.22: husarnet.cmake passes
# DOWNLOAD_EXTRACT_TIMESTAMP (3.24+), which 3.22 misreads as an extra URL entry.
include(${CMAKE_ROOT}/Modules/FetchContent.cmake)

if(CMAKE_VERSION VERSION_LESS 3.24)
  function(FetchContent_Declare)
    set(args ${ARGN})
    list(FIND args DOWNLOAD_EXTRACT_TIMESTAMP i)
    if(NOT i EQUAL -1)
      list(REMOVE_AT args ${i}) # option
      list(REMOVE_AT args ${i}) # its value
    endif()
    _FetchContent_Declare(${args})
  endfunction()
endif()
