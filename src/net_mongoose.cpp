// matth-x/MicroOcppSimulator
// Copyright Matthias Akstaller 2022 - 2024
// GPL-3.0 License

#include "net_mongoose.h"
#include "evse.h"
#include "api.h"
#include <MicroOcppMongooseClient.h>
#include <string>
#include <ArduinoJson.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Configuration.h>

//cors_headers allow the browser to make requests from any domain, allowing all headers and all methods
#define DEFAULT_HEADER "Content-Type: application/json\r\n"
#define CORS_HEADERS "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers:Access-Control-Allow-Headers, Origin,Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers\r\nAccess-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,PUT\r\n"

MicroOcpp::MOcppMongooseClient *ao_sock = nullptr;
const char *api_cert = "";
const char *api_key = "";
const char *api_user = "";
const char *api_pass = "";

void server_initialize(MicroOcpp::MOcppMongooseClient *osock, const char *cert, const char *key, const char *user, const char *pass) {
    ao_sock = osock;
    api_cert = cert;
    api_key = key;
    api_user = user;
    api_pass = pass;
}

bool api_check_basic_auth(const char *user, const char *pass) {
    if (strcmp(api_user, user)) {
        return false;
    }
    if (strcmp(api_pass, pass)) {
        return false;
    }
    return true;
}

void http_serve(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_ACCEPT) {
        if (mg_url_is_ssl((const char*)c->fn_data)) {  // TLS listener!
            MO_DBG_VERBOSE("API TLS setup");
            struct mg_tls_opts opts = {0};
            opts.cert = mg_str(api_cert);
            opts.key = mg_str(api_key);
            mg_tls_init(c, &opts);
        }
    } else if (ev == MG_EV_HTTP_MSG) {
        //struct mg_http_message *message_data = (struct mg_http_message *) ev_data;
        struct mg_http_message *message_data = reinterpret_cast<struct mg_http_message *>(ev_data);
        const char *final_headers = DEFAULT_HEADER CORS_HEADERS;

        char user[64], pass[64];
        mg_http_creds(message_data, user, sizeof(user), pass, sizeof(pass));
        if (!api_check_basic_auth(user, pass)) {
            mg_http_reply(c, 401, final_headers, "Unauthorized. Expect Basic Auth user and / or password\n");
            return;
        }

        struct mg_str json = message_data->body;

        MO_DBG_VERBOSE("%.*s", 20, message_data->uri.buf);

        MicroOcpp::Method method = MicroOcpp::Method::UNDEFINED;

        if (!mg_strcasecmp(message_data->method, mg_str("POST"))) {
            method = MicroOcpp::Method::POST;
            MO_DBG_VERBOSE("POST");
        } else if (!mg_strcasecmp(message_data->method, mg_str("GET"))) {
            method = MicroOcpp::Method::GET;
            MO_DBG_VERBOSE("GET");
        }

        //start different api endpoints
        if(mg_match(message_data->uri, mg_str("/api/websocket"), NULL)){
            MO_DBG_VERBOSE("query websocket");
            auto webSocketPingIntervalInt = MicroOcpp::declareConfiguration<int>("WebSocketPingInterval", 10, MO_WSCONN_FN);
            auto reconnectIntervalInt = MicroOcpp::declareConfiguration<int>(MO_CONFIG_EXT_PREFIX "ReconnectInterval", 30, MO_WSCONN_FN);
                    
            if (method == MicroOcpp::Method::POST) {
                if (auto val = mg_json_get_str(json, "$.backendUrl")) {
                    ao_sock->setBackendUrl(val);
                }
                if (auto val = mg_json_get_str(json, "$.chargeBoxId")) {
                    ao_sock->setChargeBoxId(val);
                }
                if (auto val = mg_json_get_str(json, "$.authorizationKey")) {
                    ao_sock->setAuthKey(val);
                }
                ao_sock->reloadConfigs();
                {
                    auto val = mg_json_get_long(json, "$.pingInterval", -1);
                    if (val > 0) {
                        webSocketPingIntervalInt->setInt(val);
                    }
                }
                {
                    auto val = mg_json_get_long(json, "$.reconnectInterval", -1);
                    if (val > 0) {
                        reconnectIntervalInt->setInt(val);
                    }
                }
                if (auto val = mg_json_get_str(json, "$.dnsUrl")) {
                    MO_DBG_WARN("dnsUrl not implemented");
                    (void)val;
                }
                MicroOcpp::configuration_save();
            }
            StaticJsonDocument<256> doc;
            doc["backendUrl"] = ao_sock->getBackendUrl();
            doc["chargeBoxId"] = ao_sock->getChargeBoxId();
            doc["authorizationKey"] = ao_sock->getAuthKey();
            doc["pingInterval"] = webSocketPingIntervalInt->getInt();
            doc["reconnectInterval"] = reconnectIntervalInt->getInt();
            std::string serialized;
            serializeJson(doc, serialized);
            mg_http_reply(c, 200, final_headers, serialized.c_str());
            return;
        } else if (strncmp(message_data->uri.buf, "/api", strlen("api")) == 0) {
            #define RESP_BUF_SIZE 8192
            char resp_buf [RESP_BUF_SIZE];

            //replace endpoint-body separator by null
            if (char *c = strchr((char*) message_data->uri.buf, ' ')) {
                *c = '\0';
            }

            int status = 404;
            if (status == 404) {
                status = mocpp_api2_call(
                    message_data->uri.buf + strlen("/api"),
                    message_data->uri.len - strlen("/api"),
                    method,
                    message_data->query.buf,
                    message_data->query.len,
                    resp_buf, RESP_BUF_SIZE);
            }
            if (status == 404) {
                status = mocpp_api_call(
                    message_data->uri.buf + strlen("/api"),
                    method,
                    message_data->body.buf,
                    resp_buf, RESP_BUF_SIZE);
            }

            mg_http_reply(c, status, final_headers, resp_buf);
        } else if (mg_match(message_data->uri, mg_str("/"), NULL)) { //if no specific path is given serve dashboard application file
            struct mg_http_serve_opts opts;
            memset(&opts, 0, sizeof(opts));
            opts.root_dir = "./public";
            opts.extra_headers = "Content-Type: text/html\r\nContent-Encoding: gzip\r\n";
            mg_http_serve_file(c, message_data, "public/bundle.html.gz", &opts);
        } else {
            mg_http_reply(c, 404, final_headers, "API endpoint not found");
        }
    }
}
