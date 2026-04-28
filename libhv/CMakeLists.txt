find_package(hv)

add_executable(
  hv_client
  hv_client.cpp
)

target_link_libraries(
  hv_client
  PRIVATE
  hv_static
  common
)

target_include_directories(
  hv_client
  PUBLIC
  ${PROJECT_SOURCE_DIR}
)
