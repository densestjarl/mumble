# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
mumble_add_client_test(TestFileTransferUi TestFileTransferUi.cpp)
add_test(NAME TestFileTransferUi COMMAND $<TARGET_FILE:TestGuiRunner> TestFileTransferUi -platform offscreen)
set_tests_properties(TestFileTransferUi PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
set_property(TEST TestFileTransferUi APPEND PROPERTY ENVIRONMENT
	"MUMBLE_QR_FIXTURE_EXPORT=${CMAKE_BINARY_DIR}/test-results/safety-qr")
mumble_client_test_runtime(TestFileTransferUi)
