# matth-x/MicroOcpp
# Copyright Matthias Akstaller 2019 - 2023
# MIT License

cmake_minimum_required(VERSION 3.13)

set(MOCPP_SM_SRC
    src/evse.cpp
    src/main.cpp
    src/webserver.cpp
    lib/mongoose/mongoose.c
)

project(MicroOcppSimulator
        VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(mo_simulator ${MOCPP_SM_SRC})

target_include_directories(mo_simulator PUBLIC
                           "${PROJECT_DIR}/lib/ArduinoJson/src"
                           "${PROJECT_DIR}/lib/mongoose"
                           )

add_compile_definitions(
    MG_ENABLE_OPENSSL=1
    MOCPP_NUMCONNECTORS=3
    MOCPP_TRAFFIC_OUT
    MOCPP_DBG_LEVEL=MOCPP_DL_INFO
    MOCPP_FILENAME_PREFIX="./mo_store/"
    )

add_subdirectory(lib/ArduinoOcpp)
target_link_libraries(mo_simulator PUBLIC MicroOcpp)

add_subdirectory(lib/ArduinoOcppMongoose)
target_link_libraries(mo_simulator PUBLIC MicroOcppMongoose)

target_link_libraries(mo_simulator PUBLIC ssl crypto)
