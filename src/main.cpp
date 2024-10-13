// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#include <iostream>
#include <signal.h>

#include <mbedtls/platform.h>

#include <MicroOcpp.h>
#include <MicroOcpp/Core/Context.h>
#include <MicroOcpp/Core/FilesystemUtils.h>
#include "evse.h"
#include "api.h"

#include <MicroOcpp/Core/Memory.h>

#if MO_NUMCONNECTORS == 3
std::array<Evse, MO_NUMCONNECTORS - 1> connectors {{1,2}};
#else
std::array<Evse, MO_NUMCONNECTORS - 1> connectors {{1}};
#endif

bool g_isOcpp201 = false;
bool g_runSimulator = true;

bool g_isUpAndRunning = false; //if the initial BootNotification and StatusNotifications got through + 1s delay
unsigned int g_bootNotificationTime = 0;

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

#if MBEDTLS_PLATFORM_MEMORY //configure MbedTLS with allocation hook functions

void *mo_mem_mbedtls_calloc( size_t n, size_t count ) {
    size_t size = n * count;
    auto ptr = MO_MALLOC("MbedTLS", size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
void mo_mem_mbedtls_free( void *ptr ) {
    MO_FREE(ptr);
}

#endif //MBEDTLS_PLATFORM_MEMORY

void mo_sim_sig_handler(int s){

    if (!g_runSimulator) { //already tried to shut down, now force stop
        exit(EXIT_FAILURE);
    }

    g_runSimulator = false; //shut down simulator gracefully
}

/*
 * Setup MicroOcpp and API
 */
void load_ocpp_version(std::shared_ptr<MicroOcpp::FilesystemAdapter> filesystem) {

    MicroOcpp::configuration_init(filesystem);

    #if MO_ENABLE_V201
    {
        auto protocolVersion_stored = MicroOcpp::declareConfiguration<const char*>("OcppVersion", "1.6", SIMULATOR_FN, false, false, false);
        MicroOcpp::configuration_load(SIMULATOR_FN);
        if (!strcmp(protocolVersion_stored->getString(), "2.0.1")) {
            //select OCPP 2.0.1
            g_isOcpp201 = true;
            return;
        }
    }
    #endif //MO_ENABLE_V201

    g_isOcpp201 = false;
}

void app_setup(MicroOcpp::Connection& connection, std::shared_ptr<MicroOcpp::FilesystemAdapter> filesystem) {
    mocpp_initialize(connection,
            g_isOcpp201 ?
                ChargerCredentials::v201("MicroOcpp Simulator", "MicroOcpp") :
                ChargerCredentials("MicroOcpp Simulator", "MicroOcpp"),
            filesystem,
            false,
            g_isOcpp201 ?
                MicroOcpp::ProtocolVersion{2,0,1} :
                MicroOcpp::ProtocolVersion{1,6}
            );

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

#ifndef MO_SIM_ENDPOINT_URL
#define MO_SIM_ENDPOINT_URL "http://0.0.0.0:8000" //URL to forward to mg_http_listen(). Will be ignored if the URL field exists in api.jsn
#endif

int main() {

#if MBEDTLS_PLATFORM_MEMORY
    mbedtls_platform_set_calloc_free(mo_mem_mbedtls_calloc, mo_mem_mbedtls_free);
#endif //MBEDTLS_PLATFORM_MEMORY

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = mo_sim_sig_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    mg_log_set(MG_LL_INFO);                            
    mg_mgr_init(&mgr);

    auto filesystem = MicroOcpp::makeDefaultFilesystemAdapter(MicroOcpp::FilesystemOpt::Use_Mount_FormatOnFail);

    load_ocpp_version(filesystem);

    struct mg_str api_cert = mg_file_read(&mg_fs_posix, MO_FILENAME_PREFIX "api_cert.pem");
    struct mg_str api_key = mg_file_read(&mg_fs_posix, MO_FILENAME_PREFIX "api_key.pem");

    auto api_settings_doc = MicroOcpp::FilesystemUtils::loadJson(filesystem, MO_FILENAME_PREFIX "api.jsn", "Simulator");
    if (!api_settings_doc) {
        api_settings_doc = MicroOcpp::makeJsonDoc("Simulator", 0);
    }
    JsonObject api_settings = api_settings_doc->as<JsonObject>();

    const char *api_url = api_settings["url"] | MO_SIM_ENDPOINT_URL;

    mg_http_listen(&mgr, api_url, http_serve, (void*)api_url);     // Create listening connection

    osock = new MicroOcpp::MOcppMongooseClient(&mgr,
        "ws://echo.websocket.events",
        "charger-01",
        "",
        "",
        filesystem,
        g_isOcpp201 ?
            MicroOcpp::ProtocolVersion{2,0,1} :
            MicroOcpp::ProtocolVersion{1,6}
        );

    server_initialize(osock, api_cert.buf ? api_cert.buf : "", api_key.buf ? api_key.buf : "", api_settings["user"] | "", api_settings["pass"] | "");
    app_setup(*osock, filesystem);

    setOnResetExecute([] (bool isHard) {
        g_runSimulator = false;
    });

    while (g_runSimulator) { //Run Simulator until OCPP Reset is executed or user presses Ctrl+C
        mg_mgr_poll(&mgr, 100);
        app_loop();

        if (!g_bootNotificationTime && getOcppContext()->getModel().getClock().now() >= MicroOcpp::MIN_TIME) {
            //time has been set, BootNotification succeeded
            g_bootNotificationTime = mocpp_tick_ms();
        }

        if (!g_isUpAndRunning && g_bootNotificationTime && mocpp_tick_ms() - g_bootNotificationTime >= 1000) {
            printf("[Sim] Resetting maximum heap usage after boot success\n");
            g_isUpAndRunning = true;
            MO_MEM_RESET();
        }
    }

    printf("[Sim] Shutting down Simulator\n");

    MO_MEM_PRINT_STATS();

    mocpp_deinitialize();

    delete osock;
    mg_mgr_free(&mgr);
    free(api_cert.buf);
    free(api_key.buf);
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
