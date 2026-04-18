add_executable(parser ${CMAKE_CURRENT_LIST_DIR}/parser.cpp)
target_link_libraries(parser PRIVATE uwebsockets::uwebsockets)
target_compile_options(parser PRIVATE -march=native)

add_executable(load_test_websocket ${CMAKE_CURRENT_LIST_DIR}/load_test_websocket.c)
target_link_libraries(load_test_websocket PRIVATE uwebsockets::uwebsockets stdc++)

add_executable(ws_echo_server ${CMAKE_CURRENT_LIST_DIR}/ws_echo_server.cpp)
target_link_libraries(ws_echo_server PRIVATE uwebsockets::uwebsockets stdc++)
