#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detail/chrono.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <locale>
#include <string>

#include "common/cmd.h"

namespace beast = boost::beast;  // from <boost/beast.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;  // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

namespace {

struct BenchData {
  int responses = 0;
  std::size_t response_bytes = 0;
  std::string host;
  uint16_t port;
  std::string request_data;
};

// Start the asynchronous operation
net::awaitable<void> run_client(BenchData* bench_data) {
  auto executor = co_await net::this_coro::executor;

  beast::flat_buffer buffer;

  websocket::stream<beast::tcp_stream> ws(executor);

  // Set a timeout on the operation
  beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));

  auto endpoint = tcp::endpoint(boost::asio::ip::make_address(bench_data->host), bench_data->port);

  // Make the connection on the IP address we get from a lookup
  co_await beast::get_lowest_layer(ws).async_connect(endpoint);

  beast::get_lowest_layer(ws).expires_never();

  auto host = bench_data->host + ':' + std::to_string(bench_data->port);

  ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

  co_await ws.async_handshake(host, "/");

  while (true) {
    co_await ws.async_write(net::buffer(bench_data->request_data));
    auto bytes_transferred = co_await ws.async_read(buffer);

    ++bench_data->responses;
    bench_data->response_bytes += bytes_transferred;
  }
}

template <typename Callback>
net::awaitable<void> run_interval(net::chrono::seconds interval, Callback callback) {
  auto executor = co_await net::this_coro::executor;
  net::steady_timer timer(executor);

  while (true) {
    timer.expires_after(interval);
    co_await timer.async_wait();
    callback();
  }
}

void run(const CmdOpt& cmd) {
  net::io_context ioc;

  BenchData bench_data{
      .responses = 0,
      .response_bytes = 0ul,
      .host = cmd.host,
      .port = cmd.port,
      .request_data = std::string(cmd.payload_size_bytes, 'a'),
  };

  for (int i = 0; i < cmd.connections; ++i) {
    net::co_spawn(ioc, run_client(&bench_data), [](std::exception_ptr e) {
      if (e) {
        std::rethrow_exception(e);
      }
    });
  }

  std::cout << "Running benchmark now...\n";

  auto interval = net::chrono::seconds(cmd.interval);
  net::co_spawn(
      ioc,
      run_interval(interval,
                   [&]() {
                     std::cout << "Msg/sec: "
                               << static_cast<float>(bench_data.responses) / static_cast<float>(interval.count())
                               << " (" << bench_data.response_bytes / (1024ul * interval.count()) << " KiB/sec)"
                               << "\n";
                     bench_data.responses = 0;
                     bench_data.response_bytes = 0ul;
                   }),
      [](std::exception_ptr e) {
        if (e) {
          std::rethrow_exception(e);
        }
      });

  ioc.run();
}

}  // namespace

int main(int argc, const char* const argv[]) {
  auto cmd = parse_opt(argc, argv);
  if (!cmd) {
    return EXIT_FAILURE;
  }

  std::cout.imbue(std::locale("en_US.UTF-8"));

  if (cmd->deflate) {
    std::cerr << "gzip-compression is not supported\n";
    return EXIT_FAILURE;
  }

  run(cmd.value());

  return EXIT_SUCCESS;
}
