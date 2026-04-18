#include <libwebsockets.h>
#include <locale.h>
#include <signal.h>

#include "common/cmd.h"

namespace {

static const char* const PROTOCOL_NAME = "echo-ws-protocol";

struct send_buffer {
  unsigned char header[LWS_PRE];
  unsigned char payload[4096];
};

// Struct to store per-connection data
struct per_session_data__echo_ws {
  struct lws* wsi;
  int connection_index;
  send_buffer buffer;
};

// Struct to store per-vhost, per-protocol data
struct vhd__echo_ws {
  struct lws_context* context;
  struct lws_vhost* vhost;
  struct lws* client_wsi;  // last wsi being created
  lws_sorted_usec_list_t sul;
  int* interrupted;
  const CmdOpt* cmd_opt;
  int connections_to_open;
  int responses;
  std::size_t response_bytes;
};

static void sul_stats_timer_cb(lws_sorted_usec_list_t* sul) {
  struct vhd__echo_ws* vhd = lws_container_of(sul, struct vhd__echo_ws, sul);

  // Do work here
  lwsl_user("Msg/sec: %f (%'ld Kib/sec)\n", ((float)vhd->responses) / vhd->cmd_opt->interval,
            vhd->response_bytes / (1024 * vhd->cmd_opt->interval));
  vhd->responses = 0;
  vhd->response_bytes = 0ul;

  // Reschedule if needed
  lws_sul_schedule(vhd->context, 0, &vhd->sul, sul_stats_timer_cb, vhd->cmd_opt->interval * LWS_USEC_PER_SEC);
}

static void next_connection(struct vhd__echo_ws* vhd) {
  if (vhd->connections_to_open--) {
    const CmdOpt* cmd_opt = vhd->cmd_opt;
    std::string host = cmd_opt->host + ':' + std::to_string(cmd_opt->port);

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = vhd->context;
    ccinfo.address = cmd_opt->host.c_str();
    ccinfo.port = (int)cmd_opt->port;
    ccinfo.path = "/";
    ccinfo.host = host.c_str();
    ccinfo.origin = ccinfo.host;
    ccinfo.ssl_connection = 0;
    if (cmd_opt->secure) {
      ccinfo.ssl_connection |= LCCSCF_USE_SSL;
    }
    ccinfo.vhost = vhd->vhost;
    // ccinfo.iface = *vhd->iface;
    ccinfo.protocol = PROTOCOL_NAME;
    ccinfo.pwsi = &vhd->client_wsi;

    auto* wsi = lws_client_connect_via_info(&ccinfo);
    if (wsi == nullptr) {
      *vhd->interrupted = 5;
    }
  } else {
    lwsl_user("Running benchmark now...");
    lws_sul_schedule(vhd->context, 0, &vhd->sul, sul_stats_timer_cb, vhd->cmd_opt->interval * LWS_US_PER_SEC);
  }
}

static int callback_client(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
  struct per_session_data__echo_ws* pss = (struct per_session_data__echo_ws*)user;
  struct vhd__echo_ws* vhd = (struct vhd__echo_ws*)lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));

  switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT: {
      vhd = (struct vhd__echo_ws*)lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi), lws_get_protocol(wsi),
                                                              sizeof(struct vhd__echo_ws));
      if (!vhd) {
        return -1;
      }

      vhd->context = lws_get_context(wsi);
      vhd->vhost = lws_get_vhost(wsi);

      /* get the pointer to "interrupted" we were passed in pvo */
      vhd->interrupted = (int*)lws_pvo_search((const struct lws_protocol_vhost_options*)in, "interrupted")->value;

      vhd->cmd_opt = (const CmdOpt*)lws_pvo_search((const struct lws_protocol_vhost_options*)in, "cmd_opt")->value;

      vhd->connections_to_open = vhd->cmd_opt->connections;
      vhd->responses = 0;
      vhd->response_bytes = 0ul;

      next_connection(vhd);
    } break;

    case LWS_CALLBACK_PROTOCOL_DESTROY:
      lws_sul_cancel(&vhd->sul);
      break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      lwsl_user("Client established\n");
      next_connection(vhd);
      lws_callback_on_writable(wsi);
      break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
      // lwsl_user("LWS_CALLBACK_CLIENT_WRITEABLE\n");
      // masking 때문에 깨져있다...
      size_t message_len = vhd->cmd_opt->payload_size_bytes;
      auto& buffer = pss->buffer;
      memset(buffer.payload, 'a', message_len);
      int sent_bytes = lws_write(wsi, buffer.payload, message_len, LWS_WRITE_TEXT);
      if (sent_bytes < (int)message_len) {
        lwsl_err("ERROR %d writing to ws socket\n", sent_bytes);
        return -1;
      }
    } break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
      // lwsl_user("LWS_CALLBACK_CLIENT_RECEIVE: %4d (rpp %5d, first %d, last %d, bin %d)\n",
      //           (int)len, (int)lws_remaining_packet_payload(wsi),
      //           lws_is_first_fragment(wsi),
      //           lws_is_final_fragment(wsi),
      //           lws_frame_is_binary(wsi));

      ++vhd->responses;
      vhd->response_bytes += len;

      /* lwsl_hexdump_notice(in, len); */
      lws_callback_on_writable(wsi);
      break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      lwsl_err("CLIENT_CONNECTION_ERROR: %s\n", in ? (char*)in : "(null)");
      vhd->client_wsi = NULL;
      if (*vhd->interrupted == 0) {
        *vhd->interrupted = 3;
      }
      lws_cancel_service(lws_get_context(wsi));
      break;

    case LWS_CALLBACK_CLIENT_CLOSED:
      lwsl_user("LWS_CALLBACK_CLIENT_CLOSED\n");

      vhd->client_wsi = NULL;
      if (*vhd->interrupted == 0) {
        *vhd->interrupted = 1;
      }
      lws_cancel_service(lws_get_context(wsi));
      break;

    default:
      break;
  }
  return 0;
}

// Define protocols
static struct lws_protocols protocols[] = {
    {
        .name = PROTOCOL_NAME,
        .callback = callback_client,
        .per_session_data_size = sizeof(struct per_session_data__echo_ws),
        .rx_buffer_size = 1024,
        .id = 0,
        .user = NULL,
        .tx_packet_size = 0,
    },
    LWS_PROTOCOL_LIST_TERM,
};

static int interrupted = 0;

void sigint_handler(int sig) {
  interrupted = 1;
}

int run(const CmdOpt& cmd) {
  lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_USER, NULL);

  struct lws_protocol_vhost_options pvo_cmd = {
      NULL,
      NULL,
      "cmd_opt",
      (const char*)&cmd,
  };

  struct lws_protocol_vhost_options pvo_interrupted = {
      &pvo_cmd,  // next linked-list
      NULL,  // child linked-list
      "interrupted", /* pvo name */
      (const char*)&interrupted /* pvo value */
  };

  struct lws_protocol_vhost_options pvo = {
      NULL, /* "next" pvo linked-list */
      &pvo_interrupted, /* "child" pvo linked-list */
      PROTOCOL_NAME, /* protocol name we belong to on this vhost */
      "" /* ignored */
  };

  struct lws_extension extensions[] = {
      {"permessage-deflate", lws_extension_callback_pm_deflate,
       "permessage-deflate"
       "; client_no_context_takeover"
       "; client_max_window_bits"},
      {NULL, NULL, NULL /* terminator */},
  };

  struct lws_context_creation_info info;

  memset(&info, 0, sizeof info); /* otherwise uninitialized garbage */
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.pvo = &pvo;
  if (cmd.deflate) {
    info.extensions = extensions;
  }
  info.pt_serv_buf_size = 32 * 1024;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  /*
   * since we know this lws context is only ever going to be used with
   * one client wsis / fds / sockets at a time, let lws know it doesn't
   * have to use the default allocations for fd tables up to ulimit -n.
   * It will just allocate for 1 internal and 1 (+ 1 http2 nwsi) that we
   * will use.
   */
  info.fd_limit_per_thread = 1 + 1 + cmd.connections;

  signal(SIGINT, sigint_handler);

  struct lws_context* context = lws_create_context(&info);
  if (!context) {
    lwsl_err("lws init failed\n");
    return EXIT_FAILURE;
  }

  // Main loop to service connections
  while (lws_service(context, 0) >= 0 && !interrupted) {
    ;
  }

  lws_context_destroy(context);

  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, const char* const argv[]) {
  auto cmd = parse_opt(argc, argv);
  if (!cmd) {
    return EXIT_FAILURE;
  }

  setlocale(LC_NUMERIC, "");

  return run(cmd.value());
}
