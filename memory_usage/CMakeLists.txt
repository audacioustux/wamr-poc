cmake_minimum_required(VERSION 3.16)
project(memory_usage)

###
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

string (TOLOWER ${CMAKE_HOST_SYSTEM_NAME} WAMR_BUILD_PLATFORM)
if (APPLE)
  add_definitions(-DBH_PLATFORM_DARWIN)
endif ()

set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

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

# set(WAMR_BUILD_INTERP 1)
# set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 1)
# set(WAMR_BUILD_JIT 1)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
# set(WAMR_BUILD_MULTI_MODULE 1)
# set(WAMR_BUILD_SIMD 1)

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
add_library(vmlib ${WAMR_RUNTIME_LIB_SOURCE})

###
message(CHECK_START "Detecting WASI-SDK")
if(NOT (DEFINED WASI_SDK_DIR OR DEFINED CACHE{WASI_SDK_DIR}))
  find_path(WASI_SDK_PARENT
    wasi-sdk
    PATHS /opt
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH
  )
  if(WASI_SDK_PARENT)
    set(WASI_SDK_DIR ${WASI_SDK_PARENT}/wasi-sdk)
  endif()
endif()
if(WASI_SDK_DIR)
  message(CHECK_PASS "found")
else()
  message(CHECK_FAIL "not found")
endif()

message(CHECK_START "Detecting WASI_TOOLCHAIN_FILE at ${WASI_SDK_DIR}")
find_file(WASI_TOOLCHAIN_FILE
  wasi-sdk.cmake
  PATHS "${WASI_SDK_DIR}/share/cmake"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
if(WASI_TOOLCHAIN_FILE)
  message(CHECK_PASS "found")
else()
  message(CHECK_FAIL "not found")
endif()

message(CHECK_START "Detecting WASI_SYS_ROOT at ${WASI_SDK_DIR}")
find_path(WASI_SYS_ROOT
  wasi-sysroot
  PATHS "${WASI_SDK_DIR}/share"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
)
if(WASI_SYS_ROOT)
  message(CHECK_PASS "found")
  set(WASI_SYS_ROOT ${WASI_SYS_ROOT}/wasi-sysroot)
else()
  message(CHECK_FAIL "not found")
endif()

if((NOT EXISTS ${WASI_SDK_DIR}) OR (NOT EXISTS ${WASI_TOOLCHAIN_FILE}) OR (NOT EXISTS ${WASI_SYS_ROOT}))
  message(FATAL_ERROR "Please set the absolute path of wasi-sdk with \'cmake -DWASI_SDK_DIR=XXX\'")
else()
  message(STATUS "WASI_SDK_DIR is ${WASI_SDK_DIR}")
  message(STATUS "WASI_TOOLCHAIN_FILE is ${WASI_TOOLCHAIN_FILE}")
  message(STATUS "WASI_SYS_ROOT is ${WASI_SYS_ROOT}")
endif()

message(CHECK_START "Detecting WAMRC in ${WAMR_ROOT_DIR}/wamr-compiler/build/")
if(${WAMR_BUILD_AOT} EQUAL 1)
  ## locate wamrc
  find_program(WAMRC
    wamrc
    PATHS ${WAMR_ROOT_DIR}/wamr-compiler/build/
  )

  if(NOT WAMRC)
    message(SEND_ERROR "can not find wamrc.  refer to \
        https://github.com/bytecodealliance/wasm-micro-runtime#build-wamrc-aot-compiler"
    )
  endif()
endif()

### .c -> .wasm
include(ExternalProject)
ExternalProject_Add(WASM_MODULE
  SOURCE_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/wasm-apps
  UPDATE_COMMAND     ""
  PATCH_COMMAND      ""
  CONFIGURE_COMMAND  ${CMAKE_COMMAND}
                       -DWASI_SDK_PREFIX=${WASI_SDK_DIR}
                       -DCMAKE_TOOLCHAIN_FILE=${WASI_TOOLCHAIN_FILE}
                       -DCMAKE_SYSROOT=${WASI_SYS_ROOT}
                       -S ${CMAKE_CURRENT_SOURCE_DIR}/wasm-apps
  BUILD_COMMAND      ${CMAKE_COMMAND} --build .
  INSTALL_COMMAND    ${CMAKE_COMMAND} -E copy
                       ./hello-world.wasm ${CMAKE_CURRENT_SOURCE_DIR}/build/
)

###
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
include(${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)

add_executable(${PROJECT_NAME} src/main.c ${UNCOMMON_SHARED_SOURCE})
add_dependencies(${PROJECT_NAME} vmlib WASM_MODULE)
target_link_libraries(${PROJECT_NAME} vmlib -lpthread -lm)