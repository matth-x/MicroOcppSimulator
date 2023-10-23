# matth-x/MicroOcpp
# Copyright Matthias Akstaller 2019 - 2023
# MIT License

cmake_minimum_required(VERSION 3.13)

set(MO_SIM_SRC
    src/evse.cpp
    src/main.cpp
    src/api.cpp
)

set(MO_SIM_MG_SRC
    src/net_mongoose.cpp
    lib/mongoose/mongoose.c
)

set(MO_SIM_WASM_SRC
    src/net_wasm.cpp
)

project(MicroOcppSimulator
        VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_definitions(
    MO_PLATFORM=MO_PLATFORM_UNIX
    MO_NUMCONNECTORS=3
    MO_TRAFFIC_OUT
    MO_DBG_LEVEL=MO_DL_INFO
    MO_FILENAME_PREFIX="./mo_store/"
)

add_executable(mo_simulator ${MO_SIM_SRC} ${MO_SIM_MG_SRC})

target_include_directories(mo_simulator PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/ArduinoJson/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/mongoose"
)

target_compile_definitions(mo_simulator PUBLIC
    MO_NETLIB=MO_NETLIB_MONGOOSE
    MG_ENABLE_OPENSSL=1
)

add_subdirectory(lib/MicroOcpp)
target_link_libraries(mo_simulator PUBLIC MicroOcpp)

add_subdirectory(lib/MicroOcppMongoose)
target_link_libraries(mo_simulator PUBLIC MicroOcppMongoose)

target_link_libraries(mo_simulator PUBLIC ssl crypto)

# experimental WebAssembly port
add_executable(mo_simulator_wasm ${MO_SIM_SRC} ${MO_SIM_WASM_SRC})

target_include_directories(mo_simulator_wasm PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/ArduinoJson/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/MicroOcppMongoose/src"
)

target_compile_definitions(mo_simulator_wasm PUBLIC
   MO_NETLIB=MO_NETLIB_WASM
)

target_link_options(mo_simulator_wasm PUBLIC
    -lwebsocket.js
    -sENVIRONMENT=web
    -sEXPORT_NAME=createModule
    -sUSE_ES6_IMPORT_META=0
    -sEXPORTED_FUNCTIONS=_main,_mocpp_wasm_api_call
    -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
    -Os
)

target_link_libraries(mo_simulator_wasm PUBLIC MicroOcpp)

if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(CMAKE_EXECUTABLE_SUFFIX ".mjs")
endif()

if(WIN32)
  target_link_libraries(mo_simulator PUBLIC wsock32 ws2_32)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
endif()
