# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
add_executable(TestServerHandlerCrypt TestServerHandlerCrypt.cpp)
set_target_properties(TestServerHandlerCrypt PROPERTIES AUTOMOC ON)
target_link_libraries(TestServerHandlerCrypt PRIVATE mumble_client_lib Qt6::Test)
add_test(NAME TestServerHandlerCrypt COMMAND $<TARGET_FILE:TestServerHandlerCrypt> -platform offscreen)
set_tests_properties(TestServerHandlerCrypt PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
mumble_client_test_runtime(TestServerHandlerCrypt)
