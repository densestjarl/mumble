# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

mumble_add_client_test(TestFileTransferManager TestFileTransferManager.cpp)
add_test(NAME TestFileTransferManager COMMAND $<TARGET_FILE:mumble-client-tests> TestFileTransferManager)
set_tests_properties(TestFileTransferManager PROPERTIES TIMEOUT 120 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

mumble_client_test_runtime(TestFileTransferManager)
