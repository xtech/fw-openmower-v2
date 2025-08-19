//
// Created by clemens on 24.06.25.
//

#include "api.hpp"
// clang-format off
#include "ch.h"
#include "hal.h"
// clang-format on
#include <mongoose.h>

#include <filesystem/file.hpp>

static THD_WORKING_AREA(waRestApi, 4096);

// HTTP server event handler function
static void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_match(hm->method, mg_str("POST"), NULL)) {
      if (mg_match(hm->uri, mg_str("/api/robot_type"), NULL)) {
        File robot_type;
        robot_type.open("robot_type.txt");
        robot_type.write(hm->body.buf, hm->body.len);
        robot_type.close();
        mg_http_reply(c, 200, "", "OK");
      } else {
        mg_http_reply(c, 500, NULL, "\n");
      }
    } else if (mg_match(hm->method, mg_str("GET"), NULL)) {
      if (mg_match(hm->uri, mg_str("/api/robot_type"), NULL)) {
        File robot_type;
        if (robot_type.open("robot_type.txt") == LFS_ERR_OK) {
          char *name[32]{};
          robot_type.read(name, sizeof(name));
          mg_http_reply(c, 200, "", "type=%s", name);
          robot_type.close();
        } else {
          mg_printf(c, "NOT_SET");
        }
      } else {
        mg_http_reply(c, 500, NULL, "\n");
      }
    }
  }
}

void RestApiThreadFunc(void *) {
  while (1) {
    struct mg_mgr mgr;                                              // Declare event manager
    mg_mgr_init(&mgr);                                              // Initialise event manager
    mg_http_listen(&mgr, "http://0.0.0.0:8000", ev_handler, NULL);  // Setup listener
    for (;;) {                                                      // Run an infinite event loop
      mg_mgr_poll(&mgr, 1000);
    }
  }
}

void InitRestAPI() {
  // Create a multicast sender thread
  thread_t *threadPointer = chThdCreateStatic(waRestApi, sizeof(waRestApi), LOWPRIO, RestApiThreadFunc, NULL);
  threadPointer->name = "REST API";
}
