# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
mumble_add_client_test(TestFileTransferMainWindow TestFileTransferMainWindow.cpp
	"${CMAKE_SOURCE_DIR}/themes/DefaultTheme.qrc")
set_target_properties(TestFileTransferMainWindow PROPERTIES AUTORCC ON)
add_test(NAME TestFileTransferMainWindow COMMAND $<TARGET_FILE:TestGuiRunner> TestFileTransferMainWindow -platform offscreen)
# Four TLS rows allow 193s each for bounded startup, transfer and cleanup,
# plus the 30s document row and fixture preparation/scheduling headroom.
set_tests_properties(TestFileTransferMainWindow PROPERTIES TIMEOUT 900 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
if(TARGET mumble-server)
	target_compile_definitions(TestFileTransferMainWindow PRIVATE
		MUMBLE_TEST_MURMUR_BINARY="$<TARGET_FILE:mumble-server>")
	add_dependencies(TestFileTransferMainWindow mumble-server)
endif()
mumble_client_test_runtime(TestFileTransferMainWindow)
