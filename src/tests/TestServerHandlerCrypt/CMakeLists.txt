# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
mumble_add_client_test(TestServerHandlerCrypt TestServerHandlerCrypt.cpp)
add_test(NAME TestServerHandlerCrypt COMMAND $<TARGET_FILE:TestGuiRunner> TestServerHandlerCrypt -platform offscreen)
set_tests_properties(TestServerHandlerCrypt PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
mumble_client_test_runtime(TestServerHandlerCrypt)
