#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct CmdOpt {
  std::string host;
  uint16_t port;
  bool secure;
  std::size_t connections;
  bool deflate;
  std::size_t payload_size_bytes;
  unsigned int interval;
};

std::optional<CmdOpt> parse_opt(int argc, const char* const argv[]);

std::string get_ws_url(const CmdOpt&);
