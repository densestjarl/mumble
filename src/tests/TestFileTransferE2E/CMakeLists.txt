# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE/>.
#
# End-to-end file-transfer test: spawns a real murmur on a scratch port and
# runs two full Mumble client connections (TLS + protobuf protocol, no GUI)
# whose engines exchange a real transfer through the server's relay path.

add_executable(TestFileTransferE2E
	TestFileTransferE2E.cpp
)

set_target_properties(TestFileTransferE2E PROPERTIES AUTOMOC ON)

target_include_directories(TestFileTransferE2E PRIVATE
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/mumble"
	"${CMAKE_BINARY_DIR}/src"
)

find_package(Qt6 COMPONENTS Sql Network REQUIRED)

target_link_libraries(TestFileTransferE2E PRIVATE shared Qt6::Test Qt6::Sql Qt6::Network)
target_link_libraries(TestFileTransferE2E PRIVATE mumble_file_transfer_test_support)

target_compile_definitions(TestFileTransferE2E PRIVATE
	MUMBLE_TEST_MURMUR_BINARY="$<TARGET_FILE:mumble-server>"
)

add_test(NAME TestFileTransferE2E COMMAND $<TARGET_FILE:TestFileTransferE2E>)
# Hosted runners start without CTest timing history. Start this long suite first
# so shorter suites can fill the other parallel slot while its transfers run.
set_tests_properties(TestFileTransferE2E PROPERTIES TIMEOUT 600 COST 300)
