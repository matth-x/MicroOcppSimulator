// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#ifndef MO_NET_WASM_H
#define MO_NET_WASM_H

#if MO_NETLIB == MO_NETLIB_WASM

#include <MicroOcpp/Core/Connection.h>

MicroOcpp::Connection *wasm_ocpp_connection_init(const char *backend_url_default, const char *charge_box_id_default, const char *auth_key_default);

#endif //MO_NETLIB == MO_NETLIB_WASM

#endif
