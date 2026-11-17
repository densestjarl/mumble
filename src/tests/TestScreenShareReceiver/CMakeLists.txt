# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
mumble_add_client_test(TestScreenShareReceiver TestScreenShareReceiver.cpp)
add_test(NAME TestScreenShareReceiver COMMAND $<TARGET_FILE:TestGuiRunner> TestScreenShareReceiver -platform offscreen)
mumble_client_test_runtime(TestScreenShareReceiver)
