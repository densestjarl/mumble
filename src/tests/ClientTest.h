// Copyright The Mumble Developers. All rights reserved.
// BSD-style license: see LICENSE at the source root.

#ifndef MUMBLE_TESTS_CLIENTTEST_H_
#define MUMBLE_TESTS_CLIENTTEST_H_

#include <QStringList>
#include <QTest>

// Keep Qt's application setup, but call its generated main from the same
// translation unit. The exported entry has a consistent signature across
// compilers (MSVC decorates Qt's array parameter differently from char **).
#define MUMBLE_CLIENT_TEST_MAIN(TestObject)                        \
	namespace mumble::tests::TestObject##Entry {                   \
		QTEST_MAIN(TestObject)                                     \
	}                                                              \
	int mumbleTestMain_##TestObject(int argc, char **argv) {       \
		return mumble::tests::TestObject##Entry::main(argc, argv); \
	}

// Relaunch the same suite when a fixture starts applicationFilePath(). Each
// child still runs the suite's original main, including its custom child modes.
// Internal linkage keeps the compile-time suite name local to its translation
// unit when all suites are linked into the same executable.
static inline QStringList clientTestArguments(QStringList arguments) {
	arguments.prepend(QStringLiteral(MUMBLE_CLIENT_TEST_NAME));
	return arguments;
}

#endif
