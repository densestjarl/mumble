// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "Connection.h"
#include "Mumble.pb.h"
#include <QtCore/QScopeGuard>
#include <QtCore/QtEndian>

#include <QtNetwork/QTcpServer>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <limits>
#include <memory>
#include <sstream>

#include <spdlog/sinks/ostream_sink.h>

// A real connected socket with controllable plaintext backlog. Avoid timing
// assumptions about kernel send buffers in the boundary/lifetime checks.
class QueuedSocket : public QSslSocket {
public:
	qint64 pending = 0;
	int writes     = 0;
	qint64 bytesToWrite() const override { return pending; }

protected:
	qint64 writeData(const char *, qint64 size) override {
		++writes;
		pending += size;
		return size;
	}
};

class TestConnectionOutputQueue : public QObject {
	Q_OBJECT
private slots:
	void admitsExactBoundary();
	void pausedReadRetainsFrameAndResumes();
	void pausedReadTimesOutOnce();
	void congestedReadsShareDeadlineAndDeliverPing();
	void congestionDeadlineRecoversAfterIdle();
	void oversizedReadHeaderClosesBeforeBody_data();
	void oversizedReadHeaderClosesBeforeBody();
	void admitsLargestProtocolFrame();
	void disconnectedSocketDoesNotWrite();
	void refusesFurtherWritesBeforeDeferredClose();
	void unboundedDefault();
	void emptyWriteDoesNotDisconnect();
	void largeQueueDoesNotOverflow();
	void destroyingConnectionCancelsDeferredClose();

private:
	static QueuedSocket *connectedSocket(QTcpServer &server) {
		if (!server.listen(QHostAddress::LocalHost))
			return nullptr;
		auto *socket = new QueuedSocket;
		socket->connectToHost(QHostAddress::LocalHost, server.serverPort());
		if (!socket->waitForConnected(1000)) {
			delete socket;
			return nullptr;
		}
		return socket;
	}
};

void TestConnectionOutputQueue::pausedReadRetainsFrameAndResumes() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	QVERIFY(server.waitForNewConnection(1000));
	auto *peer = server.nextPendingConnection();
	QVERIFY(peer);
	Connection connection(nullptr, socket);
	bool admitted = false;
	connection.setReadAdmission([&](Mumble::Protocol::TCPMessageType, qint64) {
		return admitted ? Connection::ReadAdmission::Ready : Connection::ReadAdmission::Defer;
	});
	QList< QByteArray > received;
	connect(&connection, &Connection::message, this, [&](Mumble::Protocol::TCPMessageType, const QByteArray &data) {
		received.append(data);
	});
	MumbleProto::FileData message;
	message.set_transfer_id(std::string(16, 't'));
	message.set_chunk_index(0);
	message.set_data(std::string(1024 * 1024, 'x'));
	QByteArray wire;
	QVERIFY(Connection::messageToNetwork(message, Mumble::Protocol::TCPMessageType::FileData, wire));
	QCOMPARE(peer->write(wire + wire), qint64(wire.size() * 2));
	peer->flush();
	QTest::qWait(100);
	QVERIFY(received.isEmpty());
	QVERIFY(socket->bytesAvailable() > 0);
	QCOMPARE(socket->readBufferSize(), qint64(8 * 1024 * 1024));
	admitted = true;
	QTRY_COMPARE_WITH_TIMEOUT(received.size(), 2, 5000);
	QCOMPARE(received[0], wire.mid(6));
	QCOMPARE(received[1], wire.mid(6));
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void TestConnectionOutputQueue::pausedReadTimesOutOnce() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	QVERIFY(server.waitForNewConnection(1000));
	auto *peer = server.nextPendingConnection();
	QVERIFY(peer);
	Connection connection(nullptr, socket);
	connection.setReadAdmission(
		[](Mumble::Protocol::TCPMessageType, qint64) { return Connection::ReadAdmission::Defer; }, 50);
	int timeouts = 0;
	connect(
		&connection, &Connection::messageRejected, this,
		[&](Mumble::Protocol::TCPMessageType, const QByteArray &) { ++timeouts; }, Qt::QueuedConnection);
	MumbleProto::FileData message;
	message.set_transfer_id(std::string(16, 't'));
	message.set_chunk_index(0);
	message.set_data("timeout");
	QByteArray wire;
	QVERIFY(Connection::messageToNetwork(message, Mumble::Protocol::TCPMessageType::FileData, wire));
	QCOMPARE(peer->write(wire), qint64(wire.size()));
	peer->flush();
	QTRY_COMPARE_WITH_TIMEOUT(timeouts, 1, 5000);
	QVERIFY(!connection.readPaused());
	QTest::qWait(100);
	QCOMPARE(timeouts, 1);
}

void TestConnectionOutputQueue::congestedReadsShareDeadlineAndDeliverPing() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	QVERIFY(server.waitForNewConnection(1000));
	auto *peer = server.nextPendingConnection();
	Connection connection(nullptr, socket);
	connection.setReadAdmission(
		[](Mumble::Protocol::TCPMessageType type, qint64) {
			return type == Mumble::Protocol::TCPMessageType::FileData ? Connection::ReadAdmission::Defer
																	  : Connection::ReadAdmission::Bypass;
		},
		200);
	int rejected = 0, pings = 0;
	connect(
		&connection, &Connection::messageRejected, this,
		[&](Mumble::Protocol::TCPMessageType, const QByteArray &) { ++rejected; }, Qt::QueuedConnection);
	connect(&connection, &Connection::message, this, [&](Mumble::Protocol::TCPMessageType type, const QByteArray &) {
		QCOMPARE(type, Mumble::Protocol::TCPMessageType::Ping);
		++pings;
	});
	MumbleProto::FileData data;
	data.set_transfer_id(std::string(16, 't'));
	data.set_chunk_index(0);
	data.set_data("congested");
	QByteArray file, ping;
	QVERIFY(Connection::messageToNetwork(data, Mumble::Protocol::TCPMessageType::FileData, file));
	QVERIFY(Connection::messageToNetwork(MumbleProto::Ping(), Mumble::Protocol::TCPMessageType::Ping, ping));
	QByteArray stream;
	for (int i = 0; i < 8; ++i)
		stream += file + ping;
	QCOMPARE(peer->write(stream), qint64(stream.size()));
	peer->flush();
	QTRY_VERIFY_WITH_TIMEOUT(connection.readPaused(), 100);
	QTRY_COMPARE_WITH_TIMEOUT(pings, 8, 700);
	QTRY_COMPARE_WITH_TIMEOUT(rejected, 8, 100);
	QVERIFY(!connection.readPaused());
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void TestConnectionOutputQueue::congestionDeadlineRecoversAfterIdle() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	QVERIFY(server.waitForNewConnection(1000));
	auto *peer = server.nextPendingConnection();
	Connection connection(nullptr, socket);
	bool ready = false;
	connection.setReadAdmission(
		[&](Mumble::Protocol::TCPMessageType, qint64) {
			return ready ? Connection::ReadAdmission::Ready : Connection::ReadAdmission::Defer;
		},
		100);
	int rejected = 0, received = 0;
	connect(&connection, &Connection::messageRejected, this,
			[&](Mumble::Protocol::TCPMessageType, const QByteArray &) { ++rejected; });
	connect(&connection, &Connection::message, this,
			[&](Mumble::Protocol::TCPMessageType, const QByteArray &) { ++received; });
	MumbleProto::FileData data;
	data.set_transfer_id(std::string(16, 't'));
	data.set_chunk_index(0);
	data.set_data("small final chunk");
	QByteArray wire;
	QVERIFY(Connection::messageToNetwork(data, Mumble::Protocol::TCPMessageType::FileData, wire));
	QCOMPARE(peer->write(wire), qint64(wire.size()));
	peer->flush();
	QTRY_VERIFY_WITH_TIMEOUT(connection.readPaused(), 100);
	// A delayed event loop must not renew the deadline of the same held frame.
	QTest::qSleep(150);
	QCoreApplication::processEvents();
	QCOMPARE(rejected, 1);
	QTest::qWait(150);
	QCOMPARE(peer->write(wire), qint64(wire.size()));
	peer->flush();
	QTRY_VERIFY_WITH_TIMEOUT(connection.readPaused(), 100);
	QCOMPARE(rejected, 1);
	ready = true;
	QTRY_COMPARE(received, 1);
	QCOMPARE(rejected, 1);
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void TestConnectionOutputQueue::oversizedReadHeaderClosesBeforeBody_data() {
	QTest::addColumn< quint32 >("length");
	QTest::newRow("oversized") << quint32(0x800000);
	QTest::newRow("negative") << quint32(0xffffffff);
}

void TestConnectionOutputQueue::oversizedReadHeaderClosesBeforeBody() {
	std::ostringstream output;
	auto sink   = std::make_shared< spdlog::sinks::ostream_sink_mt >(output);
	auto logger = std::make_shared< spdlog::logger >("ConnectionTest", sink);
	logger->set_pattern("%v");
	const auto previousLogger = spdlog::default_logger();
	spdlog::set_default_logger(logger);
	const auto restoreLogger = qScopeGuard([previousLogger]() { spdlog::set_default_logger(previousLogger); });
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	QVERIFY(server.waitForNewConnection(1000));
	auto *peer = server.nextPendingConnection();
	QVERIFY(peer);
	Connection connection(nullptr, socket);
	connection.setReadAdmission(
		[](Mumble::Protocol::TCPMessageType, qint64) { return Connection::ReadAdmission::Defer; });
	QByteArray header(6, '\0');
	qToBigEndian< quint16 >(static_cast< quint16 >(Mumble::Protocol::TCPMessageType::FileData), header.data());
	QFETCH(quint32, length);
	qToBigEndian< quint32 >(length, header.data() + 2);
	QCOMPARE(peer->write(header), qint64(6));
	peer->flush();
	QTRY_COMPARE_WITH_TIMEOUT(socket->state(), QAbstractSocket::UnconnectedState, 5000);
	QVERIFY(output.str().find("127.0.0.1:") != std::string::npos);
	QVERIFY(output.str().find("Host tried to send huge or invalid packet") != std::string::npos);
}

void TestConnectionOutputQueue::admitsExactBoundary() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	Connection connection(nullptr, socket);
	connection.setMaxPendingSendBytes(10);
	socket->pending = 7;
	connection.sendMessage(QByteArray(3, 'x'));
	QCOMPARE(socket->writes, 1);
	QCOMPARE(socket->pending, 10);
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void TestConnectionOutputQueue::admitsLargestProtocolFrame() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	Connection connection(nullptr, socket);
	connection.setMaxPendingSendBytes(16 * 1024 * 1024);
	// Existing messageToNetwork permits this payload plus the six-byte header.
	connection.sendMessage(QByteArray(0x7fffff + 6, 'x'));
	QCOMPARE(socket->writes, 1);
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void TestConnectionOutputQueue::disconnectedSocketDoesNotWrite() {
	auto *socket = new QueuedSocket;
	Connection connection(nullptr, socket);
	connection.setMaxPendingSendBytes(1);
	connection.sendMessage(QByteArray(2, 'x'));
	QCOMPARE(socket->writes, 0);
	QCOMPARE(socket->state(), QAbstractSocket::UnconnectedState);
}

void TestConnectionOutputQueue::refusesFurtherWritesBeforeDeferredClose() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	Connection connection(nullptr, socket);
	QSignalSpy closed(&connection, &Connection::connectionClosed);
	connection.setMaxPendingSendBytes(10);
	socket->pending = 8;
	connection.sendMessage(QByteArray(3, 'x'));
	// Even if the backlog drains or policy changes before the callback, do
	// not enqueue more frames after deciding to close this connection.
	socket->pending = 0;
	connection.setMaxPendingSendBytes(0);
	connection.sendMessage(QByteArray(1, 'x'));
	QCOMPARE(socket->writes, 0);
	QCOMPARE(closed.count(), 0);
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
	QTest::ignoreMessage(QtWarningMsg, "Outgoing TCP queue exceeded connection limit; disconnecting slow peer");
	QTRY_COMPARE(closed.count(), 1);
	QCOMPARE(socket->state(), QAbstractSocket::UnconnectedState);
}

void TestConnectionOutputQueue::unboundedDefault() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	Connection connection(nullptr, socket);
	socket->pending = 64 * 1024 * 1024;
	connection.sendMessage(QByteArray(1, 'x'));
	QCOMPARE(socket->writes, 1);
}

void TestConnectionOutputQueue::emptyWriteDoesNotDisconnect() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	Connection connection(nullptr, socket);
	connection.setMaxPendingSendBytes(1);
	socket->pending = 2;
	connection.sendMessage(QByteArray());
	QCOMPARE(socket->writes, 0);
	QCoreApplication::processEvents();
	QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
}

void TestConnectionOutputQueue::largeQueueDoesNotOverflow() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	Connection connection(nullptr, socket);
	connection.setMaxPendingSendBytes(std::numeric_limits< qint64 >::max());
	socket->pending = std::numeric_limits< qint64 >::max();
	connection.sendMessage(QByteArray(1, 'x'));
	QCOMPARE(socket->writes, 0);
	QTest::ignoreMessage(QtWarningMsg, "Outgoing TCP queue exceeded connection limit; disconnecting slow peer");
	QTRY_COMPARE(socket->state(), QAbstractSocket::UnconnectedState);
}

void TestConnectionOutputQueue::destroyingConnectionCancelsDeferredClose() {
	QTcpServer server;
	auto *socket = connectedSocket(server);
	QVERIFY(socket);
	auto *connection = new Connection(nullptr, socket);
	connection->setMaxPendingSendBytes(1);
	connection->sendMessage(QByteArray(2, 'x'));
	QCOMPARE(socket->writes, 0);
	delete connection;
	QCoreApplication::processEvents();
}

QTEST_GUILESS_MAIN(TestConnectionOutputQueue)
#include "TestConnectionOutputQueue.moc"
