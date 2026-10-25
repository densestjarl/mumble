# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.
mumble_add_client_test(TestAudioWizard TestAudioWizard.cpp)
add_test(NAME TestAudioWizard COMMAND $<TARGET_FILE:mumble-client-tests> TestAudioWizard -platform offscreen)
set_tests_properties(TestAudioWizard PROPERTIES TIMEOUT 30 WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
mumble_client_test_runtime(TestAudioWizard)
