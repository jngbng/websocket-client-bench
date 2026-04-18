#include <chrono>
#include <iostream>
#include <locale>
#include <magic_enum/magic_enum.hpp>
#include <string>

#include "common/cmd.h"
#include "wspp/wspp.h"

template <>
struct magic_enum::customize::enum_range<wspp::detail::ws_close_code> {
  static constexpr int min = 1000;
  static constexpr int max = 1009;
  // (max - min) must be less than UINT16_MAX.
};

namespace {

struct openssl_client_no_verify_policy {
  static SSL_CTX* create_ctx() {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    return ctx;
  }
};

using wss_client_no_verify =
    wspp::detail::basic_client<wspp::detail::async_tls_connector<openssl_client_no_verify_policy>,
                               wspp::detail::tls_transport<wspp::detail::openssl_client_handshake>>;

struct BenchData {
  int responses = 0;
  std::string_view url;
  std::vector<std::uint8_t> request_data;
};

template <typename Client>
int init_client(Client& client, BenchData& config) {
  auto connect_result = client.connect(config.url);
  if (connect_result != wspp::detail::ws_connect_result::ok) {
    std::cout << "Connect failed with " << magic_enum::enum_name(connect_result) << "\n";
    return EXIT_FAILURE;
  }

  client.on_message([&](wspp::message_view msg) {
    ++config.responses;
    client.send(config.request_data);
  });

  client.on_close([](wspp::close_event e) {
    // e.reason: aborted, normal, remote
    std::cout << "client close with reason[" << magic_enum::enum_name(e.reason) << "]";
    if (e.code) {
      std::cout << " and with code[" << magic_enum::enum_name(e.code.value()) << "]";
    }
    std::cout << '\n';
  });

  client.on_open([&] {
    std::cout << "client opened\n";
    client.send(config.request_data);
  });

  return EXIT_SUCCESS;
}

template <typename Client>
void run(const CmdOpt& cmd) {
  std::string url = get_ws_url(cmd);

  std::vector<Client> clients(cmd.connections);

  BenchData bench_data{
      .responses = 0,
      .url = url,
      .request_data = std::vector<std::uint8_t>(cmd.payload_size_bytes, static_cast<std::uint8_t>('a')),
  };

  for (auto& client : clients) {
    init_client(client, bench_data);
  }

  std::cout << "Running benchmark now...\n";

  auto interval = std::chrono::seconds(cmd.interval);
  auto timeout_at = std::chrono::steady_clock::now() + interval;

  while (true) {
    for (auto& client : clients) {
      client.poll();
    }
    auto now = std::chrono::steady_clock::now();
    if (now > timeout_at) [[unlikely]] {
      std::cout << "Msg/sec: " << static_cast<float>(bench_data.responses) / static_cast<float>(interval.count())
                << " (" << bench_data.responses * bench_data.request_data.size() / (1024 * interval.count())
                << " KiB/sec)"
                << "\n";

      bench_data.responses = 0;
      timeout_at = now + interval;
    }
  }
}

}  // namespace

int main(int argc, const char* const argv[]) {
#ifndef WSPP_USE_OPENSSL
  std::cout << "WSS not supported (OpenSSL disabled)\n";
  return EXIT_FAILURE;
#else

  auto cmd = parse_opt(argc, argv);
  if (!cmd) {
    return EXIT_FAILURE;
  }

  std::cout.imbue(std::locale("en_US.UTF-8"));

  if (cmd->deflate) {
    std::cerr << "gzip-compression is not supported\n";
    return EXIT_FAILURE;
  }

  if (cmd->secure) {
    run<wss_client_no_verify>(cmd.value());
  } else {
    run<wspp::ws_client>(cmd.value());
  }
#endif

  return EXIT_SUCCESS;
}
