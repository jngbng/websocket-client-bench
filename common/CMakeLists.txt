add_library(
  common
  ${CMAKE_CURRENT_LIST_DIR}/cmd.cpp
)

target_link_libraries(
  common
  PRIVATE
  argparse::argparse
)

target_include_directories(
  common
  PUBLIC
  ${PROJECT_SOURCE_DIR}
)
