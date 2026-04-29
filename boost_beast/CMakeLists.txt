add_executable(
  beast_client
  ${CMAKE_CURRENT_LIST_DIR}/beast_client.cpp
)

target_link_libraries(
  beast_client
  PRIVATE
  Boost::boost
  OpenSSL::SSL
  OpenSSL::Crypto
  certify::core
  common
)

target_include_directories(
  beast_client
  PUBLIC
  ${PROJECT_SOURCE_DIR}
)

add_executable(
  beast_coro_client
  ${CMAKE_CURRENT_LIST_DIR}/beast_coro_client.cpp
)

target_link_libraries(
  beast_coro_client
  PRIVATE
  Boost::boost
  OpenSSL::SSL
  OpenSSL::Crypto
  certify::core
  common
)

target_include_directories(
  beast_coro_client
  PUBLIC
  ${PROJECT_SOURCE_DIR}
)
