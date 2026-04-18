/* We simply call the root header file "App.h", giving you uWS::App and uWS::SSLApp */
#include <uWebSockets/App.h>

/* This is a simple WebSocket echo server example.
 * You may compile it with "WITH_OPENSSL=1 make" or with "make" */

/* ws->getUserData returns one of these */
struct PerSocketData {
  /* Fill with user data */
};

template <typename WsApp>
WsApp&& run() {
  return WsApp({.key_file_name = "misc/key.pem", .cert_file_name = "misc/cert.pem", .passphrase = "1234"})
      .template ws<PerSocketData>(
          "/*", {/* Settings */
                 .compression = uWS::CompressOptions(uWS::DEDICATED_COMPRESSOR | uWS::DEDICATED_DECOMPRESSOR),
                 .maxPayloadLength = 100 * 1024 * 1024,
                 .idleTimeout = 16,
                 .maxBackpressure = 100 * 1024 * 1024,
                 .closeOnBackpressureLimit = false,
                 .resetIdleTimeoutOnSend = false,
                 .sendPingsAutomatically = true,
                 /* Handlers */
                 .upgrade = nullptr,
                 .open =
                     [](auto* /*ws*/) {
                       /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */
                     },
                 .message =
                     [](auto* ws, std::string_view message, uWS::OpCode opCode) {
                       /* This is the opposite of what you probably want; compress if message is LARGER than 16 kb
                        * the reason we do the opposite here; compress if SMALLER than 16 kb is to allow for
                        * benchmarking of large message sending without compression */

                       /* Never mind, it changed back to never compressing for now */
                       ws->send(message, opCode, false);
                     },
                 .dropped =
                     [](auto* /*ws*/, std::string_view /*message*/, uWS::OpCode /*opCode*/) {
                       /* A message was dropped due to set maxBackpressure and closeOnBackpressureLimit limit */
                     },
                 .drain =
                     [](auto* /*ws*/) {
                       /* Check ws->getBufferedAmount() here */
                     },
                 .ping =
                     [](auto* /*ws*/, std::string_view) {
                       /* Not implemented yet */
                     },
                 .pong =
                     [](auto* /*ws*/, std::string_view) {
                       /* Not implemented yet */
                     },
                 .close =
                     [](auto* /*ws*/, int /*code*/, std::string_view /*message*/) {
                       /* You may access ws->getUserData() here */
                     }})
      .listen("127.0.0.1", 9001,
              [](auto* listen_socket) {
                if (listen_socket) {
                  std::cout << "Listening on port " << 9001 << std::endl;
                }
              })
      .run();
}

int main(int argc, const char* argv[]) {
  if (argc >= 2 && argv[1][0] == '1') {
    run<uWS::SSLApp>();
  } else {
    run<uWS::App>();
  }

  return EXIT_SUCCESS;
}
