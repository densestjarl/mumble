# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

add_executable(TestPQCrypto
	TestPQCrypto.cpp
)

set_target_properties(TestPQCrypto PROPERTIES AUTOMOC ON)

target_include_directories(TestPQCrypto PRIVATE
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/mumble"
	"${CMAKE_BINARY_DIR}/src"
)

target_link_libraries(TestPQCrypto PRIVATE shared Qt6::Test)
target_link_libraries(TestPQCrypto PRIVATE mumble_file_transfer_test_support)

add_test(NAME TestPQCrypto COMMAND $<TARGET_FILE:TestPQCrypto>)
set_tests_properties(TestPQCrypto PROPERTIES TIMEOUT 300)
