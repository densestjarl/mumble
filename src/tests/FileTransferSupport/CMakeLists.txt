# Copyright The Mumble Developers. All rights reserved.
# BSD-style license: see LICENSE at the source root.

# The standalone suites exercise the same backend without linking the complete
# GUI client. Compile these production sources once, retaining separate test
# executables, process isolation, and per-suite CTest configuration.
find_package(Qt6 COMPONENTS Sql REQUIRED)
add_library(mumble_file_transfer_test_support STATIC
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/crypto/Argon2Wrap.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/crypto/CanonicalCBOR.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/crypto/CryptoUtils.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/crypto/KemMLKEM768.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/crypto/Merkle.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/crypto/SigMLDSA65.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/identity/FTIdentity.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/identity/PeerTrustStore.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/engine/FTMessages.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/engine/FileTransferSession.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/engine/FTManifest.cpp"
	"${CMAKE_SOURCE_DIR}/src/mumble/PQFileTransfer/engine/FileTransferEngine.cpp"
)
set_target_properties(mumble_file_transfer_test_support PROPERTIES AUTOMOC ON)
target_include_directories(mumble_file_transfer_test_support PRIVATE
	"${CMAKE_SOURCE_DIR}/src"
	"${CMAKE_SOURCE_DIR}/src/mumble"
	"${CMAKE_BINARY_DIR}/src"
)
target_link_libraries(mumble_file_transfer_test_support PUBLIC shared Qt6::Sql oqs argon2)
