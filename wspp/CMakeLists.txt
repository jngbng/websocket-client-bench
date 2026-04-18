add_executable(
  wspp_client
  ${CMAKE_CURRENT_LIST_DIR}/wspp_client.cpp
)

target_link_libraries(
  wspp_client
  PRIVATE
  OpenSSL::SSL
  OpenSSL::Crypto
  magic_enum::magic_enum
  common
)

target_compile_definitions(
  wspp_client
  PRIVATE
  WSPP_USE_OPENSSL
)

target_include_directories(
  wspp_client
  PUBLIC
  ${PROJECT_SOURCE_DIR}
)
