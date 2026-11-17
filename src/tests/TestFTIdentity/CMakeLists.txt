# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

add_executable(TestFTIdentity
	TestFTIdentity.cpp
)

set_target_properties(TestFTIdentity PROPERTIES AUTOMOC ON)

find_package(Qt6 COMPONENTS Sql REQUIRED)

target_include_directories(TestFTIdentity PRIVATE
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/mumble"
	"${CMAKE_BINARY_DIR}/src"
)

target_link_libraries(TestFTIdentity PRIVATE shared Qt6::Test Qt6::Sql)
target_link_libraries(TestFTIdentity PRIVATE mumble_file_transfer_test_support)

add_test(NAME TestFTIdentity COMMAND $<TARGET_FILE:TestFTIdentity>)
set_tests_properties(TestFTIdentity PROPERTIES TIMEOUT 300)
