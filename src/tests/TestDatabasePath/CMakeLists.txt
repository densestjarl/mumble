# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
mumble_add_client_test(TestDatabasePath TestDatabasePath.cpp)
add_test(NAME TestDatabasePath COMMAND $<TARGET_FILE:TestGuiRunner> TestDatabasePath -platform offscreen)
set_tests_properties(TestDatabasePath PROPERTIES TIMEOUT 30)
mumble_client_test_runtime(TestDatabasePath)
