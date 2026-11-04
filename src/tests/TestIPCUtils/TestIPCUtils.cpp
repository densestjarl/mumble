// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "IPCUtils.h"

#include <QByteArray>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>

#include <filesystem>

#ifndef MUMBLE_TEST_WINDOWS
#	include <cstdlib>
#	include <unistd.h>
#endif

namespace {

#ifndef MUMBLE_TEST_WINDOWS
class EnvironmentVariableGuard {
public:
	explicit EnvironmentVariableGuard(const char *name)
		: m_name(name), m_originalValue(std::getenv(name)), m_wasSet(m_originalValue != nullptr) {}

	~EnvironmentVariableGuard() {
		if (m_wasSet) {
			setenv(m_name, m_originalValue.constData(), 1);
		} else {
			unsetenv(m_name);
		}
	}

private:
	const char *m_name;
	QByteArray m_originalValue;
	bool m_wasSet;
};
#endif

} // namespace

class TestIPCUtils : public QObject {
	Q_OBJECT
private slots:

#ifndef MUMBLE_TEST_WINDOWS
	void runtimeDirectory_data() {
		QTest::addColumn< QByteArray >("xdgValue");
		QTest::newRow("xdg-set") << QByteArray("xdg-runtime");
		QTest::newRow("xdg-unset") << QByteArray();
		QTest::newRow("xdg-empty") << QByteArray("");
	}

	void runtimeDirectory() {
		QFETCH(QByteArray, xdgValue);
		QTemporaryDir temporaryDirectory;
		QVERIFY(temporaryDirectory.isValid());

		EnvironmentVariableGuard xdgRuntimeDirGuard("XDG_RUNTIME_DIR");
		EnvironmentVariableGuard temporaryDirGuard("TMPDIR");
		const QByteArray temporaryPath = temporaryDirectory.path().toUtf8();
		QVERIFY(setenv("TMPDIR", temporaryPath.constData(), 1) == 0);
		if (xdgValue.isNull()) {
			QVERIFY(unsetenv("XDG_RUNTIME_DIR") == 0);
		} else {
			const QByteArray runtimePath = xdgValue.isEmpty() ? xdgValue : temporaryPath + '/' + xdgValue;
			QVERIFY(setenv("XDG_RUNTIME_DIR", runtimePath.constData(), 1) == 0);
		}

		std::filesystem::path expected = temporaryDirectory.path().toStdString();
#	ifdef MUMBLE_TEST_MACOS
		// macOS uses its temporary directory even when XDG_RUNTIME_DIR is set.
		expected /= "info.mumble.Mumble";
#	else
		if (xdgValue.isEmpty()) {
			expected /= "info.mumble.Mumble_" + std::to_string(getuid());
		} else {
			expected /= xdgValue.toStdString();
			expected /= "info.mumble.Mumble";
		}
#	endif
		QVERIFY(Mumble::getRuntimeDirectory() == expected);
		QVERIFY(std::filesystem::is_directory(expected));
		QVERIFY(Mumble::getSocketPath("connection") == expected / "connectionSocket");
		QVERIFY(Mumble::getOverlayPipePath() == expected / "MumbleOverlayPipe");
	}
#endif

#ifdef MUMBLE_TEST_WINDOWS
	void windowsUsesNativePipeNames() {
		QVERIFY(Mumble::getRuntimeDirectory().empty());
		QVERIFY(Mumble::getOverlayPipePath() == std::filesystem::path("MumbleOverlayPipe"));
		QVERIFY(Mumble::getOverlayPipeDevicePath() == LR"(\\.\pipe\MumbleOverlayPipe)");
		QVERIFY(Mumble::getSocketPath("connection") == std::filesystem::path("connection"));
	}
#endif
};

QTEST_MAIN(TestIPCUtils)
#include "TestIPCUtils.moc"
