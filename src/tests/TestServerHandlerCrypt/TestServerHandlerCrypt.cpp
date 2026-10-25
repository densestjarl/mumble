// Copyright The Mumble Developers. All rights reserved.
// BSD-style license: see LICENSE at the source root.
#include "ServerHandler.h"
#include "crypto/CryptStateOCB2.h"
#include "Global.h"

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <thread>

class CryptHandler : public ServerHandler {
public:
	using ServerHandler::message;
	void replaceCrypt(bool connected) {
		QMutexLocker lock(&qmCrypt);
		if (connected) {
			csCrypt = std::make_unique< CryptStateOCB2 >();
		} else {
			csCrypt.reset();
		}
	}
	std::string decryptIV() {
		QMutexLocker lock(&qmCrypt);
		return csCrypt->getDecryptIV();
	}
};

class TestServerHandlerCrypt : public QObject {
	Q_OBJECT
private slots:
	void setupAndResyncWithoutGuiDispatch() {
		CryptHandler handler;
		handler.replaceCrypt(true);
		MumbleProto::CryptSetup setup;
		setup.set_key(std::string(16, 'k'));
		setup.set_client_nonce(std::string(16, 'c'));
		setup.set_server_nonce(std::string(16, 's'));
		handler.message(Mumble::Protocol::TCPMessageType::CryptSetup,
						QByteArray::fromStdString(setup.SerializeAsString()));
		QCOMPARE(handler.decryptIV(), std::string(16, 's'));

		MumbleProto::CryptSetup resync;
		resync.set_server_nonce(std::string(16, 'r'));
		handler.message(Mumble::Protocol::TCPMessageType::CryptSetup,
						QByteArray::fromStdString(resync.SerializeAsString()));
		QCOMPARE(handler.decryptIV(), std::string(16, 'r'));
		PacketStats local, remote;
		QVERIFY(handler.getCryptStats(local, remote));
		QCOMPARE(local.resync, 1U);

		handler.replaceCrypt(false);
		// Queued/late setup and fieldless resync requests must be harmless after cleanup.
		handler.message(Mumble::Protocol::TCPMessageType::CryptSetup, QByteArray());
		handler.message(Mumble::Protocol::TCPMessageType::CryptSetup,
						QByteArray::fromStdString(setup.SerializeAsString()));
		QVERIFY(!handler.getCryptStats(local, remote));
	}

	void statisticsDuringDisconnect() {
		CryptHandler handler;
		std::thread cleanup([&handler]() {
			for (int i = 0; i < 10000; ++i) {
				handler.replaceCrypt(true);
				handler.replaceCrypt(false);
			}
		});
		bool consistent = true;
		for (int i = 0; i < 10000; ++i) {
			PacketStats local, remote;
			if (handler.getCryptStats(local, remote)) {
				consistent = consistent && local.good == 0 && remote.good == 0;
			}
		}
		cleanup.join();
		QVERIFY(consistent);
	}
};

int mumbleTestMain_TestServerHandlerCrypt(int argc, char **argv) {
	QApplication app(argc, argv);
	QTemporaryDir directory;
	if (!directory.isValid()) {
		return 1;
	}
	Global global(directory.filePath("settings.json"));
	Global::g_global_struct     = &global;
	global.s.qsDatabaseLocation = directory.filePath("mumble.sqlite");
	QFile database(global.s.qsDatabaseLocation);
	if (!database.open(QIODevice::WriteOnly)) {
		return 1;
	}
	database.close();
	TestServerHandlerCrypt test;
	return QTest::qExec(&test, argc, argv);
}

#include "TestServerHandlerCrypt.moc"
