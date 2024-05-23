// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#ifndef MO_SIM_API_H
#define MO_SIM_API_H

#include <cstddef>

namespace MicroOcpp {

enum class Method {
    GET,
    POST,
    UNDEFINED
};

}

int mocpp_api_call(const char *endpoint, MicroOcpp::Method method, const char *body, char *resp_body, size_t resp_body_size);

#endif
