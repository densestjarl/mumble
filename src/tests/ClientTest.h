// Copyright The Mumble Developers. All rights reserved.
// BSD-style license: see LICENSE at the source root.

#ifndef MUMBLE_TESTS_CLIENTTEST_H_
#define MUMBLE_TESTS_CLIENTTEST_H_

#include <QStringList>

// Relaunch the same suite when a fixture starts applicationFilePath(). Each
// child still runs the suite's original main, including its custom child modes.
// Internal linkage keeps the compile-time suite name local to its translation
// unit when all suites are linked into the same executable.
static inline QStringList clientTestArguments(QStringList arguments) {
	arguments.prepend(QStringLiteral(MUMBLE_CLIENT_TEST_NAME));
	return arguments;
}

#endif
