#include <iostream>

#include <MicroOcpp.h>
#include "evse.h"
#include "api.h"

#if MO_NUMCONNECTORS == 3
std::array<Evse, MO_NUMCONNECTORS - 1> connectors {{1,2}};
#else
std::array<Evse, MO_NUMCONNECTORS - 1> connectors {{1}};
#endif

#define MO_NETLIB_MONGOOSE 1
#define MO_NETLIB_WASM 2


#if MO_NETLIB == MO_NETLIB_MONGOOSE
#include "mongoose.h"
#include <MicroOcppMongooseClient.h>

#include "net_mongoose.h"

struct mg_mgr mgr;
MicroOcpp::MOcppMongooseClient *osock;

#elif MO_NETLIB == MO_NETLIB_WASM
#include <emscripten.h>

#include <MicroOcpp/Core/Connection.h>

#include "net_wasm.h"

MicroOcpp::Connection *conn = nullptr;

#else
#error Please ensure that build flag MO_NETLIB is set as MO_NETLIB_MONGOOSE or MO_NETLIB_WASM
#endif

/*
 * Setup MicroOcpp and API
 */
void app_setup(MicroOcpp::Connection& connection, std::shared_ptr<MicroOcpp::FilesystemAdapter> filesystem) {
    mocpp_initialize(connection,
            ChargerCredentials::v201("Demo Charger", "My Company Ltd."),
            filesystem,
            false,
            MicroOcpp::ProtocolVersion(2,0,1));

    for (unsigned int i = 0; i < connectors.size(); i++) {
        connectors[i].setup();
    }
}

/*
 * Execute one loop iteration
 */
void app_loop() {
    mocpp_loop();
    for (unsigned int i = 0; i < connectors.size(); i++) {
        connectors[i].loop();
    }
}

#if MO_NETLIB == MO_NETLIB_MONGOOSE

int main() {
    mg_log_set(MG_LL_INFO);                            
    mg_mgr_init(&mgr);

    mg_http_listen(&mgr, "0.0.0.0:8000", http_serve, NULL);     // Create listening connection

    auto filesystem = MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Use_Mount_FormatOnFail);

    osock = new MicroOcpp::MOcppMongooseClient(&mgr,
        "ws://echo.websocket.events",
        "charger-01",
        "",
        "",
        filesystem,
        MicroOcpp::ProtocolVersion(2,0,1));

    server_initialize(osock);
    app_setup(*osock, filesystem);

    for (;;) {                    // Block forever
        mg_mgr_poll(&mgr, 100);
        app_loop();
    }

    delete osock;
    mg_mgr_free(&mgr);
    return 0;
}

#elif MO_NETLIB == MO_NETLIB_WASM

int main() {

    printf("[WASM] start\n");

    auto filesystem = MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Deactivate);

    conn = wasm_ocpp_connection_init(nullptr, nullptr, nullptr);

    app_setup(*conn, filesystem);

    const int LOOP_FREQ = 10; //called 10 times per second
    const int BLOCK_INFINITELY = 0; //0 for non-blocking execution, 1 for blocking infinitely
    emscripten_set_main_loop(app_loop, LOOP_FREQ, BLOCK_INFINITELY);

    printf("[WASM] setup complete\n");
}
#endif
