# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
add_executable(TestFileTransferMainWindow TestFileTransferMainWindow.cpp
	"${CMAKE_SOURCE_DIR}/themes/DefaultTheme.qrc")
set_target_properties(TestFileTransferMainWindow PROPERTIES AUTOMOC ON AUTORCC ON)
target_include_directories(TestFileTransferMainWindow PRIVATE "${CMAKE_SOURCE_DIR}/src/mumble")
target_link_libraries(TestFileTransferMainWindow PRIVATE mumble_client_lib Qt6::Test)
add_test(NAME TestFileTransferMainWindow COMMAND $<TARGET_FILE:TestFileTransferMainWindow> -platform offscreen)
set_tests_properties(TestFileTransferMainWindow PROPERTIES TIMEOUT 240 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
if(TARGET mumble-server)
	target_compile_definitions(TestFileTransferMainWindow PRIVATE
		MUMBLE_TEST_MURMUR_BINARY="$<TARGET_FILE:mumble-server>")
	add_dependencies(TestFileTransferMainWindow mumble-server)
endif()
mumble_client_test_runtime(TestFileTransferMainWindow)
