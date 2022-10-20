#include "mongoose/mongoose.h"
#include <string>

static const char *s_http_addr = "http://localhost:8000";  // HTTP port
static const char *s_root_dir = "web_root";

//cors_headers allow the browser to make requests from any domain, allowing all headers and all methods
static std::string cors_headers = "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers:Access-Control-Allow-Headers, Origin,Accept, X-Requested-With, Content-Type, Access-Control-Request-Method, Access-Control-Request-Headers\r\nAccess-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,PUT\r\n";


char* toStringPtr(std::string cppString){
  char *cstr = new char[cppString.length() + 1];
  strcpy(cstr, cppString.c_str());
  return cstr;
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *message_data = (struct mg_http_message *) ev_data;
    static std::string default_header = "Content-Type: application/json\r\n";
    const char *final_headers = toStringPtr(default_header + cors_headers);
    struct mg_str json = message_data->body;

    //start different api endpoints
    if(mg_http_match_uri(message_data, "/api/set_backend_url")){
      //check that body contains data, minimum: "{}"
      if(json.len < 2){
        //no body so it is a preflight request
        mg_http_reply(c, 200, final_headers, "preflight");
        return;
      }
      //check that desired value exists
      if(mg_json_get_str(json, "$.url") != NULL){
        const char *backend_url = mg_json_get_str(json, "$.url");
        MG_INFO((backend_url));
        mg_http_reply(c, 200, final_headers, "{}");//dashboard requires valid json
      }else{
        //status code 400 -> Bad Request
        mg_http_reply(c, 400, final_headers, "The required parameters are not given");
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
        mg_http_reply(c, 200, final_headers, "{}");//dashboard requires valid json
      }else{
        mg_http_reply(c, 400, final_headers, "The required parameters are not given");
      }
    }

    //if no specific path is given serve dashboard application file
    else{
      struct mg_http_serve_opts opts = { .root_dir = "./public" };
      opts.extra_headers = "Content-Type: text/html\r\nContent-Encoding: gzip\r\n";
      mg_http_serve_file(c, ev_data, "public/bundle.html.gz", &opts);
    }
  }
}

int main() {
  struct mg_mgr mgr;
  //mg_log_set(MG_LL_DEBUG);                            
  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "0.0.0.0:8000", fn, NULL);     // Create listening connection
  for (;;) mg_mgr_poll(&mgr, 1000);                   // Block forever
  mg_mgr_free(&mgr);
  return 0;
}