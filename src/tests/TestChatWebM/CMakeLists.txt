# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# Mumble source tree or at <https://www.mumble.info/LICENSE>.

mumble_add_client_test(TestChatWebM TestChatWebM.cpp)

add_test(NAME TestChatWebM COMMAND $<TARGET_FILE:TestGuiRunner> TestChatWebM)
set_tests_properties(TestChatWebM PROPERTIES TIMEOUT 60)

mumble_client_test_runtime(TestChatWebM)
