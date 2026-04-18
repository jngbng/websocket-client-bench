#include "common/cmd.h"

#include <argparse/argparse.hpp>

std::optional<CmdOpt> parse_opt(int argc, const char* const argv[]) {
  CmdOpt ret;

  argparse::ArgumentParser program(argv[0]);

  program.add_argument("--host")
      .help("hostname to connect")
      .metavar("HOST")
      .default_value<std::string>("127.0.0.1")
      .store_into(ret.host);

  program.add_argument("-p", "--port")
      .help("port to connect to")
      .metavar("PORT")
      .default_value(static_cast<uint16_t>(9001u))
      .scan<'u', uint16_t>()
      .store_into(ret.port);

  program.add_argument("-s", "--secure")
      .help("Use wss:// instead of ws://")
      .default_value(false)
      .implicit_value(true)
      .store_into(ret.secure);

  program.add_argument("-n", "--connections")
      .help("Number of concurrent connections")
      .metavar("CONNECTIONS")
      .default_value(500ul)
      .scan<'u', std::size_t>()
      .store_into(ret.connections);

  program.add_argument("--deflate")
      .help("Enable deflate (gzip compress)")
      .default_value(false)
      .implicit_value(true)
      .store_into(ret.deflate);

  program.add_argument("--payload")
      .help("Size of payload")
      .default_value(1024ul)
      .scan<'u', std::size_t>()
      .store_into(ret.payload_size_bytes);

  program.add_argument("--interval")
      .help("Message print interval")
      .default_value(10u)
      .scan<'u', unsigned int>()
      .store_into(ret.interval);

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception& err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    return std::nullopt;
  }

  return ret;
}

std::string get_ws_url(const CmdOpt& opt) {
  std::string url;
  if (opt.secure) {
    url = "wss://";
  } else {
    url = "ws://";
  }
  url += opt.host;
  url += ':';
  url += std::to_string(opt.port);

  return url;
}
