#ifndef MOCPP_NET_WASM_H
#define MOCPP_NET_WASM_H

#if MOCPP_NETLIB == MOCPP_NETLIB_WASM

#include <MicroOcpp/Core/Connection.h>

MicroOcpp::Connection *wasm_ocpp_connection_init(const char *backend_url_default, const char *charge_box_id_default, const char *auth_key_default);

#endif //MOCPP_NETLIB == MOCPP_NETLIB_WASM

#endif
