#ifndef MOCPP_SIM_API_H
#define MOCPP_SIM_API_H

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
