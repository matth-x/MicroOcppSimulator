#include "net_mongoose.h"
#include "evse.h"
#include "api.h"
#include <MicroOcppMongooseClient.h>
#include <string>
#include <ArduinoJson.h>
#include <MicroOcpp/Debug.h>
#include <MicroOcpp/Core/Configuration.h>

static const char *s_http_addr = "http://localhost:8000";  // HTTP port
static const char *s_root_dir = "web_root";

#define OCPP_CREDENTIALS_FN MOCPP_FILENAME_PREFIX "ws-conn.jsn"

//cors_headers allow the browser to make requests from any domain, allowing all headers and all methods
#define DEFAULT_HEADER "Content-Type: application/json\r\n"
#define CORS_HEADERS "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers:Access-Control-Allow-Headers, Origin,Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers\r\nAccess-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,PUT\r\n"

MicroOcpp::MOcppMongooseClient *ao_sock = nullptr;

void server_initialize(MicroOcpp::MOcppMongooseClient *osock) {
  ao_sock = osock;
}

char* toStringPtr(std::string cppString){
  char *cstr = new char[cppString.length() + 1];
  strcpy(cstr, cppString.c_str());
  return cstr;
}

void http_serve(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
    if (ev == MG_EV_HTTP_MSG) {
        //struct mg_http_message *message_data = (struct mg_http_message *) ev_data;
        struct mg_http_message *message_data = reinterpret_cast<struct mg_http_message *>(ev_data);
        const char *final_headers = DEFAULT_HEADER CORS_HEADERS;
        struct mg_str json = message_data->body;

        MOCPP_DBG_VERBOSE("%.*s", 20, message_data->uri.ptr);

        MicroOcpp::Method method = MicroOcpp::Method::UNDEFINED;

        if (!mg_vcasecmp(&message_data->method, "POST")) {
            method = MicroOcpp::Method::POST;
            MOCPP_DBG_VERBOSE("POST");
        } else if (!mg_vcasecmp(&message_data->method, "GET")) {
            method = MicroOcpp::Method::GET;
            MOCPP_DBG_VERBOSE("GET");
        }

        //start different api endpoints
        if(mg_http_match_uri(message_data, "/api/websocket")){
            MOCPP_DBG_VERBOSE("query websocket");
            auto webSocketPingInterval = MicroOcpp::declareConfiguration<int>("WebSocketPingInterval", (int) 10, OCPP_CREDENTIALS_FN);
            auto reconnectInterval = MicroOcpp::declareConfiguration<int>("MOCPP_ReconnectInterval", (int) 30, OCPP_CREDENTIALS_FN);
                    
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
                {
                    auto val = mg_json_get_long(json, "$.pingInterval", -1);
                    if (val > 0) {
                        *webSocketPingInterval = (int) val;
                    }
                }
                {
                    auto val = mg_json_get_long(json, "$.reconnectInterval", -1);
                    if (val > 0) {
                        *reconnectInterval = (int) val;
                    }
                }
                if (auto val = mg_json_get_str(json, "$.dnsUrl")) {
                    MOCPP_DBG_WARN("dnsUrl not implemented");
                    (void)val;
                }
                MicroOcpp::configuration_save();
            }
            StaticJsonDocument<256> doc;
            doc["backendUrl"] = ao_sock->getBackendUrl();
            doc["chargeBoxId"] = ao_sock->getChargeBoxId();
            doc["authorizationKey"] = ao_sock->getAuthKey();
            doc["pingInterval"] = (int) *webSocketPingInterval;
            doc["reconnectInterval"] = (int) *reconnectInterval;
            std::string serialized;
            serializeJson(doc, serialized);
            mg_http_reply(c, 200, final_headers, serialized.c_str());
            return;
        } else if(strncmp(message_data->uri.ptr, "/api", strlen("api")) == 0) {
            #define RESP_BUF_SIZE 1024
            char resp_buf [RESP_BUF_SIZE];

            //replace endpoint-body separator by null
            if (char *c = strchr((char*) message_data->uri.ptr, ' ')) {
                *c = '\0';
            }

            int status = mocpp_api_call(
                message_data->uri.ptr + strlen("/api"),
                method,
                message_data->body.ptr,
                resp_buf, RESP_BUF_SIZE);
            
            mg_http_reply(c, status, final_headers, resp_buf);
        }
        //if no specific path is given serve dashboard application file
        else if (mg_http_match_uri(message_data, "/")) {
        struct mg_http_serve_opts opts = { .root_dir = "./public" };
        opts.extra_headers = "Content-Type: text/html\r\nContent-Encoding: gzip\r\n";
        mg_http_serve_file(c, message_data, "public/bundle.html.gz", &opts);
        } else {
            mg_http_reply(c, 404, final_headers, "The required parameters are not given");
        }
    }
}
