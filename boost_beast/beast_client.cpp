#include <boost/asio/detail/chrono.hpp>
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

class ws_client {
  websocket::stream<beast::tcp_stream> ws_;
  beast::flat_buffer buffer_;
  BenchData* bench_data_;

 public:
  // Resolver and socket require an io_context
  explicit ws_client(net::io_context& ioc, BenchData* bench_data)
      : ws_(net::make_strand(ioc)), bench_data_(bench_data) {
  }

  // Start the asynchronous operation
  void run() {
    // Set a timeout on the operation
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));

    auto endpoint = tcp::endpoint(boost::asio::ip::make_address(bench_data_->host), bench_data_->port);

    // Make the connection on the IP address we get from a lookup
    beast::get_lowest_layer(ws_).async_connect(endpoint, beast::bind_front_handler(&ws_client::on_connect, this));
  }

  void on_connect(beast::error_code ec) {
    if (ec) {
      return fail(ec, "connect");
    }

    beast::get_lowest_layer(ws_).expires_never();

    auto host = bench_data_->host + ':' + std::to_string(bench_data_->port);

    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    ws_.async_handshake(host, "/", beast::bind_front_handler(&ws_client::on_handshake, this));
  }

  void on_handshake(beast::error_code ec) {
    if (ec) {
      return fail(ec, "handshake");
    }

    start_send();
  }

  void start_send() {
    ws_.async_write(net::buffer(bench_data_->request_data), beast::bind_front_handler(&ws_client::on_write, this));
  }

  void on_write(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
      return fail(ec, "write");
    }

    ws_.async_read(buffer_, beast::bind_front_handler(&ws_client::on_read, this));
  }

  void on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
      return fail(ec, "read");
    }

    ++bench_data_->responses;
    bench_data_->response_bytes += bytes_transferred;

    start_send();
  }

  void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << "\n";
  }
};

class interval_timer {
 public:
  explicit interval_timer(net::io_context& context, net::chrono::seconds interval)
      : timer_(context), interval_(interval) {
  }

  template <typename Callback>
  void run(Callback callback) {
    timer_.expires_after(interval_);
    timer_.async_wait([callback, this]([[maybe_unused]] boost::system::error_code ec) {
      callback();
      run(callback);
    });
  }

 private:
  net::steady_timer timer_;
  net::chrono::seconds interval_;
};

void run(const CmdOpt& cmd) {
  net::io_context ioc;

  BenchData bench_data{
      .responses = 0,
      .response_bytes = 0ul,
      .host = cmd.host,
      .port = cmd.port,
      .request_data = std::string(cmd.payload_size_bytes, 'a'),
  };

  std::vector<ws_client> clients;
  clients.reserve(cmd.connections);
  for (int i = 0; i < cmd.connections; ++i) {
    auto& client = clients.emplace_back(ioc, &bench_data);
    client.run();
  }

  std::cout << "Running benchmark now...\n";

  auto interval = net::chrono::seconds(cmd.interval);
  auto timer = interval_timer(ioc, interval);
  timer.run([&]() {
    std::cout << "Msg/sec: " << static_cast<float>(bench_data.responses) / static_cast<float>(interval.count()) << " ("
              << bench_data.response_bytes / (1024ul * interval.count()) << " KiB/sec)"
              << "\n";
    bench_data.responses = 0;
    bench_data.response_bytes = 0ul;
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
