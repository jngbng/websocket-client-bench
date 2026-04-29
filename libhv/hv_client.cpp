#include <hv/Event.h>
#include <hv/EventLoop.h>
#include <hv/WebSocketClient.h>
#include <locale.h>

#include <deque>

#include "common/cmd.h"

namespace {

using namespace hv;

struct BenchData {
  int responses = 0;
  std::string_view url;
  std::string request_data;
};

int init_client(WebSocketClient& ws, BenchData& config) {
  ws.onopen = [&]() {
    ws.send(config.request_data);
  };

  ws.onmessage = [&](const std::string& msg) {
    ++config.responses;
    ws.send(config.request_data);
  };

  ws.onclose = []() {
    printf("onclose\n");
  };

  // reconnect: 1,2,4,8,10,10,10...
  reconn_setting_t reconn;
  reconn_setting_init(&reconn);
  reconn.min_delay = 1000;
  reconn.max_delay = 10000;
  reconn.delay_policy = 2;
  ws.setReconnect(&reconn);

  ws.open(config.url.data());

  return EXIT_SUCCESS;
}

int run(const CmdOpt& cmd) {
  auto url = get_ws_url(cmd);
  BenchData bench_data{
      .responses = 0,
      .url = url,
      .request_data = std::string(cmd.payload_size_bytes, 'a'),
  };

  auto loop_thread = std::make_shared<EventLoopThread>();
  loop_thread->start();

  loop_thread->loop()->setInterval(cmd.interval * 1000, [&](hv::TimerID timer_id) {
    std::cout << "Msg/sec: " << static_cast<float>(bench_data.responses) / static_cast<float>(cmd.interval) << " ("
              << bench_data.responses * bench_data.request_data.size() / (1024 * cmd.interval) << " KiB/sec)"
              << "\n";

    bench_data.responses = 0;
  });

  std::deque<WebSocketClient> clients;
  for (int i = 0; i < cmd.connections; ++i) {
    auto& client = clients.emplace_back(loop_thread->loop());
    init_client(client, bench_data);
  }

  std::cout << "Running benchmark now...\n";

  loop_thread->join();

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
