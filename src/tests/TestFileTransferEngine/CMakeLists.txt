# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

add_executable(TestFileTransferEngine
	TestFileTransferEngine.cpp
)

set_target_properties(TestFileTransferEngine PROPERTIES AUTOMOC ON)

target_include_directories(TestFileTransferEngine PRIVATE
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/mumble"
	"${CMAKE_BINARY_DIR}/src"
)

find_package(Qt6 COMPONENTS Sql REQUIRED)

target_link_libraries(TestFileTransferEngine PRIVATE shared Qt6::Test Qt6::Sql)
target_link_libraries(TestFileTransferEngine PRIVATE mumble_file_transfer_test_support)

add_test(NAME TestFileTransferEngine COMMAND $<TARGET_FILE:TestFileTransferEngine>)
set_tests_properties(TestFileTransferEngine PROPERTIES TIMEOUT 600)
