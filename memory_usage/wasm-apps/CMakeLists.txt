### use the latest cmake and CXX_STANDARD
cmake_minimum_required(VERSION 3.18)
set(CMAKE_CXX_STANDARD 23)

project(wasm-apps)

###
# set(WASI_SDK_PREFIX ${WASI_SDK_DIR})
# set(CMAKE_TOOLCHAIN_FILE ${WASI_TOOLCHAIN_FILE})
# set(CMAKE_SYSROOT ${WASI_SYS_ROOT})

function(COMPILE_C_TO_WASM SOURCE_FILE WASI_COMMAND_MODE)
  cmake_path(GET SOURCE_FILE STEM FILE_NAME)

  set(WASM_MODULE ${FILE_NAME}.wasm)

  set(TARGET_NAME MODULE_${FILE_NAME})

  # option(CMAKE_C_FLAGS "" "${CMAKE_C_FLAGS} -nostdlib")
  # target_compile_options(target_compile_options(common INTERFACE INTERFACE
  # set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS},--export=__main_argc_argv")
  # set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -nostdlib -O3")
  # set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS},--no-entry,--shared-memory,--max-memory=196608,--export=__heap_base,--export=__data_end")

  add_executable(${TARGET_NAME} ${SOURCE_FILE})
  target_compile_options(${TARGET_NAME} PRIVATE -nostdlib)
  target_link_options(${TARGET_NAME} PRIVATE
    LINKER:--no-entry
    LINKER:--export=__heap_base
    LINKER:--export=__data_end
    LINKER:--export=__main_argc_argv
    LINKER:--shared-memory,--max-memory=196608
    LINKER:--no-check-features
  )
  set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME ${WASM_MODULE} 
                                                  COMPILE_FLAGS "-nostdlib -O3"
                                                  LINK_FLAGS_RELEASE "-Wl,--no-entry,--strip-all"
  )
  # target_compile_options(${TARGET_NAME} INTERFACE
  #   -nostdlib
  # )
  target_link_options(${TARGET_NAME} PRIVATE "-Wl,--no-entry")

  if(WASI_COMMAND_MODE)
    message(STATUS "Generating ${WASM_MODULE} as COMMAND...")
  else()
    message(STATUS "Generating ${WASM_MODULE} as REACTOR...")
    target_link_options(${TARGET_NAME} PRIVATE -mexec-model=reactor)
  endif()
  # target_link_options(${TARGET_NAME} PRIVATE "-Wl,--strip-all,--no-entry,--export=main,--export=__main_argc_argv,--export=__data_end,--export=__heap_base,--allow-undefined")

  # message(STATUS "Dumping ${WASM_MODULE}...")
  # set(WASM_WAT ${FILE_NAME}.wat)
  # set(DUMP_TARGET_NAME DUMP_${FILE_NAME})
  # add_custom_command(OUTPUT ${WASM_WAT}
  #   COMMAND ${WASM2WAT} --enable-all -o ${WASM_WAT} ${WASM_MODULE}
  #   COMMENT "Dumping ${WASM_MODULE}..."
  #   DEPENDS ${TARGET_NAME}
  # )

  # add_custom_target(${DUMP_TARGET_NAME} ALL
  #   DEPENDS ${WASM_WAT}
  # )
endfunction()

COMPILE_C_TO_WASM(hello-world.c ON)
