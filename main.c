#include "mongoose/mongoose.h"

/**
Does not work cross-origin (also if only port differs)
*/

static const char *s_http_addr = "http://localhost:8000";  // HTTP port
static const char *s_root_dir = "web_root";

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  //handle http requests
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *message_data = (struct mg_http_message *) ev_data;

    //set_backend_url api endpoint
    if(mg_http_match_uri(message_data, "/api/set_backend_url")){
      mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{%Q:%d}\n", "result", 123);
      MG_INFO(("New Backend Url!"));
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
  mg_log_set(MG_LL_DEBUG);                            
  mg_mgr_init(&mgr);
  mg_http_listen(&mgr, "0.0.0.0:8000", fn, NULL);     // Create listening connection
  for (;;) mg_mgr_poll(&mgr, 1000);                   // Block forever
  mg_mgr_free(&mgr);
  return 0;
}