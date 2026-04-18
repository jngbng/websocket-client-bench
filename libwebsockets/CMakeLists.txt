find_package(Libwebsockets REQUIRED)

add_executable(
  lws_client
  lws_client.cpp
)

target_link_libraries(
  lws_client
  PRIVATE
  websockets
  common
)

target_include_directories(
  lws_client
  PUBLIC
  ${PROJECT_SOURCE_DIR}
)
