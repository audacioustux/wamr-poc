### use the latest cmake and CXX_STANDARD
cmake_minimum_required(VERSION 3.18)
set(CMAKE_C_STANDARD 23)

project(memory_usage)

### update git submodules
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

### set build options and flags
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

string (TOLOWER ${CMAKE_HOST_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
if (APPLE)
  add_definitions(-DBH_PLATFORM_DARWIN)
endif ()

if (NOT DEFINED WAMR_BUILD_TARGET)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)")
    set (WAMR_BUILD_TARGET "AARCH64")
  elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
    set (WAMR_BUILD_TARGET "RISCV64")
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (WAMR_BUILD_TARGET "X86_64")
  elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set (WAMR_BUILD_TARGET "X86_32")
  else ()
    message(SEND_ERROR "Unsupported build target platform!")
  endif ()
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

set(WAMR_BUILD_INTERP 1)
set(WAMR_BUILD_FAST_INTERP 0)
# set(WAMR_BUILD_AOT 1)
set(WAMR_BUILD_JIT 1)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
# set(WAMR_BUILD_MULTI_MODULE 1)
# set(WAMR_BUILD_SIMD 0)
set(WAMR_BUILD_MEMORY_PROFILING 1)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie -fPIE")
if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif ()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wformat -Wformat-security")
if (WAMR_BUILD_TARGET MATCHES "X86_.*" OR WAMR_BUILD_TARGET STREQUAL "AMD_64")
  if (NOT (CMAKE_C_COMPILER MATCHES ".*clang.*" OR CMAKE_C_COMPILER_ID MATCHES ".*Clang"))
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mindirect-branch-register")
  endif ()
endif ()

###
set (WAMR_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../wasm-micro-runtime)
if(NOT EXISTS ${WAMR_ROOT_DIR})
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

include(${WAMR_ROOT_DIR}/build-scripts/runtime_lib.cmake)
add_library(vmlib STATIC ${WAMR_RUNTIME_LIB_SOURCE})

###
message(CHECK_START "Detecting WASI-SDK")
if(NOT (DEFINED WASI_SDK_DIR OR DEFINED CACHE{WASI_SDK_DIR}))
  # find_path(WASI_SDK_PARENT
  #   wasi-sdk
  #   PATHS /opt
  #   NO_DEFAULT_PATH
  #   REQUIRED
  # )
  # set(WASI_SDK_DIR ${WASI_SDK_PARENT}/wasi-sdk)
  set(WASI_SDK_DIR ../wasi-sdk-19.0)
endif()

###
message(CHECK_START "Detecting wasi-sdk bundled clang at ${WASI_SDK_DIR}/bin")
find_program(WASI_CLANG
  clang
  PATHS "${WASI_SDK_DIR}/bin"
  NO_DEFAULT_PATH
  REQUIRED
)

### Detect wamrc
message(CHECK_START "Detecting WAMRC in ${WAMR_ROOT_DIR}/wamr-compiler/build/")
if(WAMR_BUILD_AOT EQUAL 1)
  find_program(WAMRC
    wamrc
    PATHS ${WAMR_ROOT_DIR}/wamr-compiler/build/
    REQUIRED
  )
endif()

###
find_program(WAT2WASM wat2wasm REQUIRED)
###
find_program(WASM2WAT wasm2wat REQUIRED)
###
find_program(WASM_OBJDUMP wasm-objdump REQUIRED)

###
macro(DUMP_WAT_MACRO)
  cmake_path(REPLACE_EXTENSION ${prefix}_TARGET ".wat" OUTPUT_VARIABLE WASM2WAT_TARGET)
  add_custom_command(
    TARGET ${NAME} POST_BUILD
    COMMAND ${WASM2WAT} --enable-all ${${prefix}_TARGET} -o ${WASM2WAT_TARGET}
    DEPENDS ${WASM2WAT}
    DEPENDS ${${prefix}_TARGET}
    BYPRODUCTS ${WASM2WAT_TARGET}
    VERBATIM
  )
endmacro()

macro(DUMP_AOT_MACRO)
  cmake_path(REPLACE_EXTENSION ${prefix}_TARGET ".aot" OUTPUT_VARIABLE WASM2AOT_TARGET)
  add_custom_command(
    TARGET ${NAME} POST_BUILD
    COMMAND ${WAMRC} -o ${WASM2AOT_TARGET} ${${prefix}_TARGET}
    DEPENDS ${WAMRC}
    DEPENDS ${${prefix}_TARGET}
    BYPRODUCTS ${WASM2AOT_TARGET}
    VERBATIM
  )
endmacro()

function(COMPILE_C_TO_WASM NAME)
  set(prefix C2WASM)
  set(flags DUMP_AOT DUMP_WAT IS_REACTOR)
  set(singleValues TARGET SOURCE)
  set(multiValues CLANG_ARGS EXPORTS)

  include(CMakeParseArguments)
  cmake_parse_arguments(${prefix} "${flags}" "${singleValues}" "${multiValues}" ${ARGN})
  if(DEFINED ${prefix}_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "undefined arguments!")
  endif()

  foreach(EXPORT ${${prefix}_EXPORTS})
    LIST(APPEND ${prefix}_CLANG_ARGS "-Wl,--export=${EXPORT}")
  endforeach()
  if(${${prefix}_IS_REACTOR})
    LIST(APPEND ${prefix}_CLANG_ARGS "-mexec-model=reactor")
  endif()

  add_custom_target(${NAME}
    COMMAND ${WASI_CLANG} ${${prefix}_CLANG_ARGS} ${${prefix}_SOURCE} -o ${${prefix}_TARGET}
    DEPENDS ${WASI_CLANG}
    BYPRODUCTS ${${prefix}_TARGET}
    VERBATIM
  )

  if(${${prefix}_DUMP_AOT})
    DUMP_AOT_MACRO()
  endif()
  if(${${prefix}_DUMP_WAT})
    DUMP_WAT_MACRO()
  endif()
endfunction()

function(COMPILE_WAT_TO_WASM NAME)
  set(prefix WAT2WASM)
  set(flags DUMP_AOT)
  set(singleValues TARGET SOURCE)

  include(CMakeParseArguments)
  cmake_parse_arguments(${prefix} "${flags}" "${singleValues}" "${multiValues}" ${ARGN})
  if(DEFINED ${prefix}_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "undefined arguments!")
  endif()

  add_custom_target(${NAME}
    COMMAND ${WAT2WASM} ${${prefix}_SOURCE} -o ${${prefix}_TARGET}
    DEPENDS ${WAT2WASM}
    BYPRODUCTS ${${prefix}_TARGET}
    VERBATIM
  )

  if(${${prefix}_DUMP_AOT})
    DUMP_AOT_MACRO()
  endif()
endfunction()

###
include(${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)

set(TEST_EXAMPLES_C
  # hello-world.c
  noop.c
)

foreach(EX ${TEST_EXAMPLES_C})
  set(SRC ${CMAKE_CURRENT_LIST_DIR}/src/${EX})
  cmake_path(GET SRC STEM EX_NAME)

  add_executable(${EX_NAME} ${SRC} ${UNCOMMON_SHARED_SOURCE})
  target_include_directories(${EX_NAME} PRIVATE ${UNCOMMON_SHARED_DIR})
  target_link_libraries(${EX_NAME} vmlib -lpthread -lm)

  if(${WAMR_BUILD_AOT})
    set(DUMP_AOT DUMP_AOT)
  endif()

  # if(${EX_NAME} STREQUAL "hello-world")
  #   # COMPILE_C_TO_WASM(hello_world SOURCE "${CMAKE_CURRENT_LIST_DIR}/wasm-apps/hello-world.c" TARGET "${PROJECT_BINARY_DIR}/hello-world.wasm"
  #   # CLANG_ARGS -O3 -z stack-size=8192 -Wl,--initial-memory=65536,--strip-all,--no-entry
  #   # # EXPORTS __data_end __heap_base run
  #   # DUMP_WAT IS_REACTOR ${DUMP_AOT})
  #   # add_dependencies(${EX_NAME} hello_world)

  #   # COMPILE_WAT_TO_WASM(dummy_module SOURCE "${CMAKE_CURRENT_LIST_DIR}/wasm-apps/dummy-module.wat" TARGET "${PROJECT_BINARY_DIR}/dummy-module.wasm"
  #   # ${DUMP_AOT})
  #   # add_dependencies(${EX_NAME} dummy_module)

  #   # COMPILE_WAT_TO_WASM(simple_global SOURCE "${CMAKE_CURRENT_LIST_DIR}/wasm-apps/simple-global.wat" TARGET "${PROJECT_BINARY_DIR}/simple-global.wasm"
  #   # ${DUMP_AOT})
  #   # add_dependencies(${EX_NAME} simple_global)
  # endif()
endforeach()
