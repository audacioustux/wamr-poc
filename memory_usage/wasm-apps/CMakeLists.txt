### use the latest cmake and CXX_STANDARD
cmake_minimum_required(VERSION 3.18)
set(CMAKE_CXX_STANDARD 23)

project(wasm-apps)

###
find_program(WAT2WASM wat2wasm REQUIRED)
###
find_program(WASM2WAT wasm2wat REQUIRED)
###
find_program(WASM_OBJDUMP wasm-objdump REQUIRED)

###
set(WASI_SDK_PREFIX ${WASI_SDK_DIR})
set(CMAKE_TOOLCHAIN_FILE ${WASI_TOOLCHAIN_FILE})
set(CMAKE_SYSROOT ${WASI_SYS_ROOT})

function(COMPILE_C_TO_WASM SOURCE_FILE WASI_COMMAND_MODE)
  cmake_path(GET SOURCE_FILE STEM FILE_NAME)

  set(WASM_MODULE ${FILE_NAME}.wasm)

  set(MAIN_TARGET_NAME MODULE_${FILE_NAME})

  add_executable(${MAIN_TARGET_NAME} ${SOURCE_FILE})
  set_target_properties(${MAIN_TARGET_NAME} PROPERTIES OUTPUT_NAME ${WASM_MODULE})

  if(WASI_COMMAND_MODE)
    message(STATUS "Generating ${WASM_MODULE} as COMMAND...")
  else()
    message(STATUS "Generating ${WASM_MODULE} as REACTOR...")
    target_link_options(${MAIN_TARGET_NAME} PRIVATE -mexec-model=reactor)
  endif()

  if(EXISTS ${WASM2WAT})
    message(STATUS "Dumping ${WASM_MODULE}...")
    set(WASM_WAT ${FILE_NAME}.wat)
    set(DUMP_TARGET_NAME DUMP_${FILE_NAME})
    add_custom_command(OUTPUT ${WASM_WAT}
      COMMAND ${WASM2WAT} --enable-all -o ${WASM_WAT} ${WASM_MODULE}
      COMMENT "Dumping ${WASM_MODULE}..."
      DEPENDS ${MAIN_TARGET_NAME}
    )

    add_custom_target(${DUMP_TARGET_NAME} ALL
      DEPENDS ${WASM_WAT}
    )
  endif()
endfunction()

COMPILE_C_TO_WASM(hello-world.c ON)
