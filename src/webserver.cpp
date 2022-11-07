#include "webserver.h"
#include "evse.h"
#include "ws_adapter.h"
#include <string>
#include <ArduinoJson.h>
#include <ArduinoOcpp/Debug.h>

static const char *s_http_addr = "http://localhost:8000";  // HTTP port
static const char *s_root_dir = "web_root";

//cors_headers allow the browser to make requests from any domain, allowing all headers and all methods
#define DEFAULT_HEADER "Content-Type: application/json\r\n"
#define CORS_HEADERS "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers:Access-Control-Allow-Headers, Origin,Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers\r\nAccess-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,PUT\r\n"


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

    Evse *evse = nullptr;
    char cIdRaw [10];
    if (mg_http_get_var(&message_data->query, "connectorId", cIdRaw, 10) > 0) {
      int connectorId = cIdRaw[0] - '0';
      if (connectorId >= 1 && connectorId < OCPP_NUMCONNECTORS) {
        evse = &connectors[connectorId-1];
      }
    }

    //start different api endpoints
    if(mg_http_match_uri(message_data, "/api/ocpp_backend")){
      //check that body contains data, minimum: "{}"
      if (!mg_vcasecmp(&message_data->method, "POST")) {
        if(json.len < 2){
          //no body so it is a preflight request
          mg_http_reply(c, 200, final_headers, "preflight");
          return;
        }
        //check that desired value exists
        const char *backend_url = mg_json_get_str(json, "$.backendUrl");
        const char *cbId = mg_json_get_str(json, "$.chargeBoxId");
        osock.setUrl(backend_url, cbId);

        const char *auth_token = mg_json_get_str(json, "$.authToken");
        if (auth_token) {
          osock.setAuthToken(auth_token);
        }

        mg_http_reply(c, 200, final_headers, "{}");//dashboard requires valid json
      } else if (!mg_vcasecmp(&message_data->method, "GET")) {
        StaticJsonDocument<256> doc;
        AO_DBG_DEBUG("Set data");
        doc["backendUrl"] = osock.getUrlBackend();
        doc["chargeBoxId"] = osock.getChargeBoxId();
        doc["authToken"] = osock.getAuthToken();
        AO_DBG_DEBUG("Data set");
        std::string serialized;
        serializeJson(doc, serialized);
        mg_http_reply(c, 200, final_headers, serialized.c_str());
      }
    }

    //start different api endpoints
    if(mg_http_match_uri(message_data, "/api/ca_cert")){
      //check that body contains data, minimum: "{}"
      if (!mg_vcasecmp(&message_data->method, "POST")) {
        if(json.len < 2){
          //no body so it is a preflight request
          mg_http_reply(c, 200, final_headers, "preflight");
          return;
        }
        const char *ca_cert = mg_json_get_str(json, "$.caCert");
        if (ca_cert) {
          osock.setCa(ca_cert);
        }

        mg_http_reply(c, 200, final_headers, "{}");//dashboard requires valid json
      } else if (!mg_vcasecmp(&message_data->method, "GET")) {
        StaticJsonDocument<1024> doc;
        doc["caCert"] = osock.getCa();
        std::string serialized;
        serializeJson(doc, serialized);
        mg_http_reply(c, 200, final_headers, serialized.c_str());
      }
    }

    else if(mg_http_match_uri(message_data, "/api/status_ev")){
      if (!evse) {
        mg_http_reply(c, 400, final_headers, "The required parameters are not given");
        return;
      }
      if (!mg_vcasecmp(&message_data->method, "POST")) {
        if(json.len < 2){
          mg_http_reply(c, 200, final_headers, "preflight");
          return;
        }
        bool evPlugged, evReady;
        if (mg_json_get_bool(json, "$.evPlugged", &evPlugged)) {
          evse->setEvPlugged(evPlugged);
        }
        if (mg_json_get_bool(json, "$.evReady", &evReady)) {
          evse->setEvReady(evReady);
        }
        mg_http_reply(c, 200, final_headers, "{}");
      } else if (!mg_vcasecmp(&message_data->method, "GET")) {
        StaticJsonDocument<64> doc;
        doc["evPlugged"] = evse->getEvPlugged();
        doc["evReady"] = evse->getEvReady();
        std::string serialized;
        serializeJson(doc, serialized);
        mg_http_reply(c, 200, final_headers, serialized.c_str());
      } else {
        mg_http_reply(c, 400, final_headers, "Unsupported method");
      }
    }
    
    else if(mg_http_match_uri(message_data, "/api/user_authorization")){
      MG_INFO(("RFID info"));
      if (!evse) {
        mg_http_reply(c, 400, final_headers, "The required parameters are not given");
        return;
      }

      if (!mg_vcasecmp(&message_data->method, "POST")) {
        if(json.len < 2){
          mg_http_reply(c, 200, final_headers, "preflight");
          return;
        }
        const char *idTag = mg_json_get_str(json, "$.idTag");
        if (idTag) {
          evse->presentNfcTag(idTag);
          MG_INFO((idTag));
          mg_http_reply(c, 200, final_headers, "{}");//dashboard requires valid json
        } else {
          mg_http_reply(c, 400, final_headers, "The required parameters are not given");
        }
      } else if (!mg_vcasecmp(&message_data->method, "GET")) {
        StaticJsonDocument<64> doc;
        doc["idTag"] = evse->getSessionIdTag() ? evse->getSessionIdTag() : "";
        std::string serialized;
        serializeJson(doc, serialized);
        mg_http_reply(c, 200, final_headers, serialized.c_str());
      } else {
        mg_http_reply(c, 400, final_headers, "Unsupported method");
      }
    }

    else if(mg_http_match_uri(message_data, "/api/status_evse")){
      if (!evse) {
        mg_http_reply(c, 400, final_headers, "The required parameters are not given");
        return;
      }

      if (!mg_vcasecmp(&message_data->method, "GET")) {
        StaticJsonDocument<192> doc;
        doc["idTag"] = evse->getSessionIdTag() ? evse->getSessionIdTag() : "";
        doc["transactionId"] = evse->getTransactionId();
        doc["chargePermission"] = evse->chargingPermitted();
        doc["isCharging"] = evse->isCharging();
        doc["ocppStatus"] = (char*) evse->getOcppStatus();
        std::string serialized;
        serializeJson(doc, serialized);
        mg_http_reply(c, 200, final_headers, serialized.c_str());
      } else {
        mg_http_reply(c, 400, final_headers, "Unsupported method");
      }
    }

    else if(mg_http_match_uri(message_data, "/api/secondary_url")){
      if(json.len < 2){
        mg_http_reply(c, 200, final_headers, "preflight");
        return;
      }
      if(mg_json_get_str(json, "$.sec_url") != NULL){
        const char *secondary_url = mg_json_get_str(json, "$.sec_url");
        MG_INFO((secondary_url));
        mg_http_reply(c, 200, final_headers, "{}");
      }else{
        mg_http_reply(c, 400, final_headers, "The required parameters are not given");
      }
    }

    //if no specific path is given serve dashboard application file
    else{
      struct mg_http_serve_opts opts = { .root_dir = "./public" };
      opts.extra_headers = "Content-Type: text/html\r\nContent-Encoding: gzip\r\n";
      mg_http_serve_file(c, message_data, "public/bundle.html.gz", &opts);
    }
  }
}
