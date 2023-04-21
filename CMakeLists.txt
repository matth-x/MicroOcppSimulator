# matth-x/ArduinoOcpp
# Copyright Matthias Akstaller 2019 - 2023
# MIT License

cmake_minimum_required(VERSION 3.13)

set(AO_SM_SRC
    src/evse.cpp
    src/main.cpp
    src/webserver.cpp
    lib/mongoose/mongoose.c
)

project(ArduinoOcppSimulator
        VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(ao_simulator ${AO_SM_SRC})

target_include_directories(ao_simulator PUBLIC
                           "${PROJECT_DIR}/lib/ArduinoJson/src"
                           "${PROJECT_DIR}/lib/mongoose"
                           )

add_compile_definitions(
    MG_ENABLE_OPENSSL=1
    AO_NUMCONNECTORS=3
    AO_TRAFFIC_OUT
    AO_DBG_LEVEL=AO_DL_INFO
    AO_FILENAME_PREFIX="./ao_store"
    )

add_subdirectory(lib/ArduinoOcpp)
target_link_libraries(ao_simulator PUBLIC ArduinoOcpp)

add_subdirectory(lib/ArduinoOcppMongoose)
target_link_libraries(ao_simulator PUBLIC ArduinoOcppMongoose)

target_link_libraries(ao_simulator PUBLIC ssl crypto)
