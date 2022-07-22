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

# set(WAMR_BUILD_INTERP 1)
# set(WAMR_BUILD_FAST_INTERP 1)
set(WAMR_BUILD_AOT 1)
# set(WAMR_BUILD_JIT 1)
set(WAMR_BUILD_LIBC_BUILTIN 1)
set(WAMR_BUILD_LIBC_WASI 1)
# set(WAMR_BUILD_MULTI_MODULE 1)
# set(WAMR_BUILD_SIMD 0)

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
  find_path(WASI_SDK_PARENT
    wasi-sdk
    PATHS /opt
    NO_DEFAULT_PATH
    REQUIRED
  )
  set(WASI_SDK_DIR ${WASI_SDK_PARENT}/wasi-sdk)
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
set(TEST_EXAMPLES_C
  hello-world
)

include(${SHARED_DIR}/utils/uncommon/shared_uncommon.cmake)
foreach(EX ${TEST_EXAMPLES_C})
  set(SRC ${CMAKE_CURRENT_LIST_DIR}/src/${EX}.c)

  add_executable(${EX} ${SRC} ${UNCOMMON_SHARED_SOURCE})
  target_include_directories(${EX} PRIVATE ${UNCOMMON_SHARED_DIR})
  target_link_libraries(${EX} vmlib -lpthread -lm)

  set(WASM_SRC ${CMAKE_CURRENT_LIST_DIR}/wasm-apps/${EX}.c)

  if(${EX} STREQUAL "hello-world")
    set(WASI_CLANG_ARGS 
        -O3
        -z stack-size=4096 -Wl,--initial-memory=65536
        -Wl,--export=main,
        -Wl,--export=__main_argc_argv,
        -Wl,--export=__data_end,
        -Wl,--export=__heap_base
        -Wl,--strip-all,--no-entry
        -Wl,--allow-undefined
        -nostdlib)
  endif(${EX} STREQUAL "hello-world")

  add_custom_target(${EX}_WASM
    COMMAND ${WASI_CLANG} ${WASM_SRC} -o ${PROJECT_BINARY_DIR}/${EX}.wasm ${WASI_CLANG_ARGS}
    DEPENDS ${WASI_CLANG}
    BYPRODUCTS ${PROJECT_BINARY_DIR}/${EX}.wasm
    VERBATIM
    COMMENT "generate a wasm file ${PROJECT_BINARY_DIR}/${EX}.wasm"
  )
  add_dependencies(${EX} ${EX}_WASM)

  add_custom_target(${EX}_WAT
    COMMAND ${WASM2WAT} --enable-all -o ${PROJECT_BINARY_DIR}/${EX}.wat
      ${PROJECT_BINARY_DIR}/${EX}.wasm
    DEPENDS ${EX}_WASM
    BYPRODUCTS ${PROJECT_BINARY_DIR}/${EX}.wat
    VERBATIM
    COMMENT "generate a wat file ${PROJECT_BINARY_DIR}/${EX}.wat"
  )
  add_dependencies(${EX} ${EX}_WAT)

  if(${WAMR_BUILD_AOT} EQUAL 1)
    add_custom_target(${EX}_AOT
      COMMAND ${WAMRC} -o ${PROJECT_BINARY_DIR}/${EX}.aot
        ${PROJECT_BINARY_DIR}/${EX}.wasm
      DEPENDS ${EX}_WASM
      BYPRODUCTS ${PROJECT_BINARY_DIR}/${EX}.aot
      VERBATIM
      COMMENT "generate a aot file ${PROJECT_BINARY_DIR}/${EX}.aot"
    )
    add_dependencies(${EX} ${EX}_AOT)
  endif()
endforeach()
