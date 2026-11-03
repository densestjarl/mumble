// Copyright The Mumble Developers. All rights reserved.
// BSD-style license: see LICENSE at the source root.

#include "CaptureSourceLister.h"
#include "Mumble.pb.h"
#include "MumbleProtocol.h"
#include "ScreenCapture.h"
#include "ScreenShareReceiver.h"
#include "VideoFramePacketizer.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QScreen>
#include <QSslSocket>
#include <QTimer>
#include <QWidget>
#include <QtEndian>

#include <array>
#include <cstdint>
#include <dlfcn.h>
#include <sys/resource.h>

namespace {
constexpr int WarmupFrames = 15;
constexpr int TotalFrames  = 165;

double cpuSeconds() {
	rusage usage{};
	getrusage(RUSAGE_SELF, &usage);
	return static_cast< double >(usage.ru_utime.tv_sec) + static_cast< double >(usage.ru_utime.tv_usec) / 1e6
		   + static_cast< double >(usage.ru_stime.tv_sec) + static_cast< double >(usage.ru_stime.tv_usec) / 1e6;
}

class Scene : public QWidget {
public:
	int frame = 0;
	Scene() {
		setWindowFlags(Qt::FramelessWindowHint);
		setGeometry(QGuiApplication::primaryScreen()->geometry());
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter painter(this);
		painter.fillRect(rect(), QColor(25, 30, 40));
		for (int y = 0; y < height(); y += 24) {
			painter.setPen(QColor(90 + y % 120, 160, 180));
			painter.drawText(16, y + 18,
							 QStringLiteral("Mumble screen sharing: reproducible desktop content %1").arg(y));
		}
		painter.fillRect((frame * 13) % (width() - 120), 300, 120, 120, QColor(210, 90, 60));
	}
};
} // namespace

int main(int argc, char **argv) {
	QApplication app(argc, argv);
	if (argc != 4) {
		qFatal("Usage: BenchmarkVideoPipeline sender|receiver PORT OUTPUT.json");
	}
	const bool sender    = QString::fromLocal8Bit(argv[1]) == QStringLiteral("sender");
	const QString output = QString::fromLocal8Bit(argv[3]);
	QSslSocket socket;
	ScreenCapture capture;
	ScreenShareReceiver receiver;
	Scene scene;
	QTimer frameTimer, gapTimer, pingTimer;
	QByteArray incoming;
	quint32 session      = 0;
	int frames           = 0;
	quint64 payloadBytes = 0, wireBytes = 0, packets = 0;
	QJsonArray frameMS, packetizeMS, gapsMS, allocations, packetizeAllocations;
	QCryptographicHash encodedHash(QCryptographicHash::Sha256), decodedHash(QCryptographicHash::Sha256),
		inputHash(QCryptographicHash::Sha256);
	QElapsedTimer operation, elapsed, gap;
	double cpuStart            = 0;
	double decodeMS            = 0;
	using Begin                = void (*)();
	using End                  = void (*)(std::uint64_t *);
	const auto allocationBegin = reinterpret_cast< Begin >(dlsym(RTLD_DEFAULT, "video_allocations_begin"));
	const auto allocationEnd   = reinterpret_cast< End >(dlsym(RTLD_DEFAULT, "video_allocations_end"));
	if (!allocationBegin || !allocationEnd) {
		qFatal("Preload libvideo_allocations.so to measure allocations");
	}

	const auto send = [&](quint16 type, const QByteArray &data) {
		std::array< char, 6 > header{};
		qToBigEndian(type, header.data());
		qToBigEndian(static_cast< quint32 >(data.size()), header.data() + 2);
		socket.write(header.data(), header.size());
		socket.write(data);
	};
	const auto sendProto = [&](quint16 type, const google::protobuf::Message &message) {
		const std::string data = message.SerializeAsString();
		send(type, QByteArray(data.data(), static_cast< qsizetype >(data.size())));
	};
	const auto startMeasurement = [&]() {
		cpuStart = cpuSeconds();
		elapsed.start();
		gap.start();
		gapTimer.start();
	};
	const auto finish = [&]() {
		frameTimer.stop();
		gapTimer.stop();
		const double seconds = static_cast< double >(elapsed.nsecsElapsed()) / 1e9;
		rusage usage{};
		getrusage(RUSAGE_SELF, &usage);
		const QJsonObject result{ { "role", sender ? "sender" : "receiver" },
								  { "frames", frames - WarmupFrames },
								  { "seconds", seconds },
								  { "cpu_seconds", cpuSeconds() - cpuStart },
								  { "peak_rss_kib", static_cast< double >(usage.ru_maxrss) },
								  { "frame_ms", frameMS },
								  { "packetize_ms", packetizeMS },
								  { "packetize_allocations", packetizeAllocations },
								  { "event_loop_gap_ms", gapsMS },
								  { "allocations", allocations },
								  { "encoded_bytes", static_cast< double >(payloadBytes) },
								  { "tunnel_bytes", static_cast< double >(wireBytes) },
								  { "packets", static_cast< double >(packets) },
								  { "encoded_sha256", QString::fromLatin1(encodedHash.result().toHex()) },
								  { "input_rgb_sha256", QString::fromLatin1(inputHash.result().toHex()) },
								  { "decoded_sha256", QString::fromLatin1(decodedHash.result().toHex()) } };
		QFile file(output);
		if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(result).toJson()) < 0) {
			qFatal("Could not write benchmark output");
		}
		app.quit();
	};
	gapTimer.setInterval(5);
	gapTimer.setTimerType(Qt::PreciseTimer);
	QObject::connect(&gapTimer, &QTimer::timeout, &app, [&]() {
		gapsMS.append(static_cast< double >(gap.nsecsElapsed()) / 1e6);
		gap.restart();
	});
	QObject::connect(&receiver, &ScreenShareReceiver::frameDecoded, &app, [&](quint32, const QImage &image) {
		decodeMS += static_cast< double >(operation.nsecsElapsed()) / 1e6;
		std::array< std::uint64_t, 6 > counts{};
		allocationEnd(counts.data());
		if (frames >= WarmupFrames) {
			frameMS.append(decodeMS);
			QJsonArray row;
			for (std::uint64_t value : counts) {
				row.append(static_cast< double >(value));
			}
			allocations.append(row);
			// Hash the endpoints rather than spending CPU hashing every 8 MiB frame.
			// The encoded hash still covers the complete measured bitstream.
			if (frames == WarmupFrames || frames == TotalFrames - 1) {
				for (int y = 0; y < image.height(); ++y) {
					decodedHash.addData(
						QByteArrayView(reinterpret_cast< const char * >(image.constScanLine(y)), image.width() * 4));
				}
			}
		}
		decodeMS = 0;
		++frames;
	});
	QObject::connect(&capture, &ScreenCapture::frameEncoded, &app,
					 [&](const QByteArray &data, quint64 number, quint32 width, quint32 height, bool key) {
						 const double captureMS = static_cast< double >(operation.nsecsElapsed()) / 1e6;
						 std::array< std::uint64_t, 6 > counts{};
						 allocationEnd(counts.data());
						 if (frames >= WarmupFrames) {
							 QJsonArray row;
							 for (std::uint64_t value : counts) {
								 row.append(static_cast< double >(value));
							 }
							 allocations.append(row);
						 }
						 QElapsedTimer packetize;
						 allocationBegin();
						 packetize.start();
						 const std::vector< std::vector< unsigned char > > fragments =
							 Mumble::Video::packetizeFrame(session, data, number, width, height, key);
						 const double fragmentMS = static_cast< double >(packetize.nsecsElapsed()) / 1e6;
						 std::array< std::uint64_t, 6 > packetCounts{};
						 allocationEnd(packetCounts.data());
						 if (frames >= WarmupFrames) {
							 frameMS.append(captureMS);
							 packetizeMS.append(fragmentMS);
							 QJsonArray row;
							 for (std::uint64_t value : packetCounts) {
								 row.append(static_cast< double >(value));
							 }
							 packetizeAllocations.append(row);
							 payloadBytes += static_cast< quint64 >(data.size());
							 encodedHash.addData(data);
						 }
						 for (const std::vector< unsigned char > &fragment : fragments) {
							 send(1, QByteArray(reinterpret_cast< const char * >(fragment.data()),
												static_cast< qsizetype >(fragment.size())));
							 if (frames >= WarmupFrames) {
								 wireBytes += fragment.size() + 6;
								 ++packets;
							 }
						 }
					 });
	QObject::connect(&frameTimer, &QTimer::timeout, &app, [&]() {
		if (frames == WarmupFrames) {
			startMeasurement();
		}
		scene.frame = frames;
		scene.repaint();
		operation.start();
		allocationBegin();
		const bool invoked = QMetaObject::invokeMethod(&capture, "captureFrame", Qt::DirectConnection);
		std::array< std::uint64_t, 6 > counts{};
		allocationEnd(counts.data());
		if (!invoked) {
			qFatal("Could not invoke captureFrame");
		}
		if (frames == WarmupFrames || frames == TotalFrames - 1) {
			// Outside the measured capture operation; check the actual X11 scene.
			const QImage input = grabCaptureSource(CaptureSource()).convertToFormat(QImage::Format_RGB888);
			for (int y = 0; y < input.height(); ++y) {
				inputHash.addData(
					QByteArrayView(reinterpret_cast< const char * >(input.constScanLine(y)), input.width() * 3));
			}
		}
		if (++frames == TotalFrames) {
			socket.flush();
			while (socket.bytesToWrite() > 0) {
				if (!socket.waitForBytesWritten(5000)) {
					qFatal("Could not drain video packets");
				}
			}
			capture.stopCapture();
			finish();
		}
	});
	QObject::connect(&socket, &QSslSocket::readyRead, &app, [&]() {
		incoming += socket.readAll();
		while (incoming.size() >= 6) {
			const quint16 type = qFromBigEndian< quint16 >(incoming.constData());
			const quint32 size = qFromBigEndian< quint32 >(incoming.constData() + 2);
			if (incoming.size() < size + 6) {
				return;
			}
			const QByteArray data = incoming.mid(6, size);
			incoming.remove(0, size + 6);
			if (type == 5) {
				MumbleProto::ServerSync sync;
				if (!sync.ParseFromArray(data.constData(), static_cast< int >(data.size()))) {
					qFatal("Invalid ServerSync");
				}
				session = sync.session();
				QFile ready(output + QStringLiteral(".ready"));
				if (!ready.open(QIODevice::WriteOnly)) {
					qFatal("Could not write readiness marker");
				}
				if (sender) {
					MumbleProto::UserState state;
					state.set_session(session);
					state.set_screen_sharing(true);
					sendProto(9, state);
					scene.show();
					capture.startCapture();
					capture.findChild< QTimer * >()->stop();
					frameTimer.setTimerType(Qt::PreciseTimer);
					frameTimer.start(66);
				}
			} else if (type == 1 && !sender) {
				Mumble::Protocol::UDPDecoder< Mumble::Protocol::Role::Client > decoder(
					Version::fromComponents(1, 5, 0));
				if (!decoder.decode(std::span< const Mumble::Protocol::byte >(
						reinterpret_cast< const Mumble::Protocol::byte * >(data.constData()),
						static_cast< std::size_t >(data.size())))
					|| decoder.getMessageType() != Mumble::Protocol::UDPMessageType::Video) {
					continue;
				}
				if (frames == WarmupFrames && !elapsed.isValid()) {
					startMeasurement();
				}
				const int before = frames;
				operation.start();
				allocationBegin();
				receiver.handleVideoPacket(decoder.getVideoData());
				std::array< std::uint64_t, 6 > counts{};
				allocationEnd(counts.data());
				if (before == frames) {
					decodeMS += static_cast< double >(operation.nsecsElapsed()) / 1e6;
				}
				if (frames == TotalFrames) {
					finish();
				}
			}
		}
	});
	socket.setPeerVerifyMode(QSslSocket::VerifyNone);
	socket.connectToHostEncrypted(QStringLiteral("127.0.0.1"),
								  static_cast< quint16 >(QString::fromLocal8Bit(argv[2]).toUInt()));
	if (!socket.waitForEncrypted(10000)) {
		qFatal("Loopback TLS connection failed");
	}
	MumbleProto::Version version;
	version.set_version_v2(Version::fromComponents(1, 5, 0));
	version.set_release("video-pipeline-benchmark");
	sendProto(0, version);
	MumbleProto::Authenticate auth;
	auth.set_username(sender ? "sender" : "receiver");
	auth.set_opus(true);
	sendProto(2, auth);
	pingTimer.start(3000);
	QObject::connect(&pingTimer, &QTimer::timeout, &app, [&]() {
		MumbleProto::Ping ping;
		ping.set_timestamp(1);
		sendProto(3, ping);
	});
	QTimer::singleShot(30000, &app, [&]() { qFatal("Video pipeline benchmark timed out at frame %d", frames); });
	return app.exec();
}
