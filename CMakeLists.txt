# matth-x/MicroOcppSimulator
# Copyright Matthias Akstaller 2022 - 2024
# GPL-3.0 License

cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 11)

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
    MO_ENABLE_V201=1
    MO_ENABLE_MBEDTLS=1
    MO_ENABLE_TIMESTAMP_MILLISECONDS=1
    MO_MG_USE_VERSION=MO_MG_V715
)

add_executable(mo_simulator ${MO_SIM_SRC} ${MO_SIM_MG_SRC})

target_include_directories(mo_simulator PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/ArduinoJson/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/mongoose"
)

target_compile_definitions(mo_simulator PUBLIC
    MO_NETLIB=MO_NETLIB_MONGOOSE
)

add_subdirectory(lib/MicroOcpp)
target_link_libraries(mo_simulator PUBLIC MicroOcpp)

# disable some warnings for MbedTLS which cause compilation errors on WASM
if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    add_compile_options(
        -Wno-unused-but-set-variable
        -Wno-documentation
    )
endif()

# disable MbedTLS unit tests and test suites (not needed for the Simualtor)
option(ENABLE_TESTING  "Build mbed TLS tests."    OFF)
option(ENABLE_PROGRAMS "Build mbed TLS programs." OFF)

add_subdirectory(lib/mbedtls)
target_link_libraries(MicroOcpp PUBLIC 
    mbedtls
    mbedcrypto
    mbedx509
)

if (MO_SIM_BUILD_USE_OPENSSL)

    message("Using OpenSSL for WebSocket")

    # find OpenSSL
    find_package(OpenSSL REQUIRED)
    target_include_directories(mo_simulator PUBLIC
        "${OPENSSL_INCLUDE_DIR}"
    )
    target_link_libraries(mo_simulator PUBLIC
        ${OPENSSL_LIBRARIES}
    )
    target_compile_definitions(mo_simulator PUBLIC
        MG_ENABLE_OPENSSL=1
    )

else()

    message("Using MbedTLS for WebSocket")

    target_link_libraries(mo_simulator PUBLIC 
        mbedtls
        mbedcrypto
        mbedx509
    )
    target_compile_definitions(mo_simulator PUBLIC
        MG_TLS=MG_TLS_MBED
    )

endif()

add_subdirectory(lib/MicroOcppMongoose)
target_link_libraries(mo_simulator PUBLIC MicroOcppMongoose)

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
