// Copyright The Mumble Developers. All rights reserved.
// BSD-style license: see LICENSE at the source root.

#include "ClientTest.h"
#include "ScreenShareReceiver.h"

#include <QSignalSpy>

#include <algorithm>
#include <array>
#include <numeric>
#include <random>
#include <span>
#include <vector>

class TestScreenShareReceiver : public QObject {
	Q_OBJECT

	static Mumble::Protocol::VideoData packet(quint32 count = 3) {
		Mumble::Protocol::VideoData data;
		data.senderSession = 1;
		data.frameNumber   = 1;
		data.fragmentCount = count;
		data.fragmentIndex = 0;
		data.width         = 1920;
		data.height        = 1080;
		// Isolate reassembly from codec work. Completed frames still take the
		// production cleanup path, but no decoder accepts this codec.
		data.codec   = static_cast< MumbleUDP::Video::Codec >(127);
		data.payload = std::span< const Mumble::Protocol::byte >(m_payload);
		return data;
	}
	static constexpr std::array< Mumble::Protocol::byte, 1024 > m_payload{};

private slots:
	void duplicateAndOutOfOrder() {
		ScreenShareReceiver receiver;
		Mumble::Protocol::VideoData data = packet();
		data.fragmentIndex               = 2;
		receiver.handleVideoPacket(data);
		receiver.handleVideoPacket(data);
		QCOMPARE(receiver.m_fragmentBuffer.at(1).at(1).receivedBytes, m_payload.size());
		data.fragmentIndex = 0;
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).contains(1));
		data.fragmentIndex = 1;
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
	}

	void malformedMetadata() {
		ScreenShareReceiver receiver;
		Mumble::Protocol::VideoData data = packet();
		receiver.handleVideoPacket(data);
		data.width++;
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
		data               = packet();
		data.fragmentCount = 0;
		receiver.handleVideoPacket(data);
		data.fragmentCount = 4097;
		receiver.handleVideoPacket(data);
		data.fragmentCount = 3;
		data.fragmentIndex = 3;
		receiver.handleVideoPacket(data);
		data.fragmentIndex = 0;
		data.payload       = {};
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
	}

	void encodedByteLimit() {
		ScreenShareReceiver receiver;
		Mumble::Protocol::VideoData data = packet(2);
		const std::vector< Mumble::Protocol::byte > payload(4 * 1024 * 1024, 1);
		data.payload = payload;
		receiver.handleVideoPacket(data);
		QCOMPARE(receiver.m_fragmentBuffer.at(1).at(1).receivedBytes, payload.size());
		// An already stored fragment does not consume the limit a second time.
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).contains(1));
		data.fragmentIndex = 1;
		data.payload       = m_payload;
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
	}

	void boundedWindowAndReset() {
		ScreenShareReceiver receiver;
		Mumble::Protocol::VideoData data = packet();
		for (quint64 number = 1; number <= 40; ++number) {
			data.frameNumber = number;
			receiver.handleVideoPacket(data);
		}
		QCOMPARE(receiver.m_fragmentBuffer.at(1).size(), std::size_t(32));
		QCOMPARE(receiver.m_fragmentBuffer.at(1).begin()->first, quint64(9));
		data.fragmentIndex = 1;
		receiver.handleVideoPacket(data);
		data.fragmentIndex = 2;
		receiver.handleVideoPacket(data);
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
		receiver.handleVideoPacket(packet());
		receiver.resetSender(1);
		QVERIFY(receiver.m_fragmentBuffer.empty());
	}

	void decodesReorderedFrame() {
		// One 16x16 red H.264 IDR, generated using libx264 superfast/zerolatency.
		const QByteArray encoded = QByteArray::fromBase64(
			"AAAAAWdkAAqstPYCIAAAAwAgAAADA9HiRNQAAAABaO8PywAAAQYF//9a3EXpvebZSLeWLNgg2SPu73gyNjQgLSBjb3JlIDE2NCBy"
			"MzEwOCAzMWUxOWY5IC0gSC4yNjQvTVBFRy00IEFWQyBjb2RlYyAtIENvcHlsZWZ0IDIwMDMtMjAyMyAtIGh0dHA6Ly93d3cudmlk"
			"ZW9sYW4ub3JnL3gyNjQuaHRtbCAtIG9wdGlvbnM6IGNhYmFjPTEgcmVmPTEgZGVibG9jaz0xOjA6MCBhbmFseXNlPTB4MzoweDMg"
			"bWU9ZGlhIHN1Ym1lPTEgcHN5PTEgcHN5X3JkPTEuMDA6MC4wMCBtaXhlZF9yZWY9MCBtZV9yYW5nZT0xNiBjaHJvbWFfbWU9MSB0"
			"cmVsbGlzPTAgOHg4ZGN0PTEgY3FtPTAgZGVhZHpvbmU9MjEsMTEgZmFzdF9wc2tpcD0xIGNocm9tYV9xcF9vZmZzZXQ9MCB0aHJl"
			"YWRzPTEgbG9va2FoZWFkX3RocmVhZHM9MSBzbGljZWRfdGhyZWFkcz0wIG5yPTAgZGVjaW1hdGU9MSBpbnRlcmxhY2VkPTAgYmx1"
			"cmF5X2NvbXBhdD0wIGNvbnN0cmFpbmVkX2ludHJhPTAgYmZyYW1lcz0wIHdlaWdodHA9MSBrZXlpbnQ9MjUwIGtleWludF9taW49"
			"MTUgc2NlbmVjdXQ9NDAgaW50cmFfcmVmcmVzaD0wIHJjPWNyZiBtYnRyZWU9MCBjcmY9MjMuMCBxY29tcD0wLjYwIHFwbWluPTAg"
			"cXBtYXg9NjkgcXBzdGVwPTQgaXBfcmF0aW89MS40MCBhcT0xOjEuMDAAgAAAAWWIhAS/5P8F4PqBgWCN/2/KLIi9IyPB");
		ScreenShareReceiver receiver;
		QSignalSpy decoded(&receiver, &ScreenShareReceiver::frameDecoded);
		Mumble::Protocol::VideoData data = packet();
		data.codec                       = MumbleUDP::Video::H264;
		data.width = data.height         = 16;
		data.isKeyFrame                  = true;
		constexpr qsizetype fragmentSize = 32;
		data.fragmentCount               = static_cast< quint32 >((encoded.size() + fragmentSize - 1) / fragmentSize);
		for (quint32 remaining = data.fragmentCount; remaining > 0; --remaining) {
			data.fragmentIndex     = remaining - 1;
			const qsizetype offset = data.fragmentIndex * fragmentSize;
			data.payload           = std::span< const Mumble::Protocol::byte >(
                reinterpret_cast< const Mumble::Protocol::byte           *>(encoded.constData() + offset),
                static_cast< std::size_t >(qMin(fragmentSize, encoded.size() - offset)));
			receiver.handleVideoPacket(data);
			if (remaining > 1) {
				receiver.handleVideoPacket(data);
				QCOMPARE(decoded.size(), 0);
			}
		}
		QCOMPARE(decoded.size(), 1);
		const QImage image = qvariant_cast< QImage >(decoded.at(0).at(1));
		QCOMPARE(image.size(), QSize(16, 16));
		const QColor pixel = image.pixelColor(8, 8);
		QVERIFY(pixel.red() > 240);
		QVERIFY(pixel.green() < 10);
		QVERIFY(pixel.blue() < 10);
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
	}

	void reassemblyBenchmark_data() {
		QTest::addColumn< quint32 >("count");
		QTest::addColumn< int >("order");
		for (quint32 count : { 8u, 128u, 1024u, 4096u }) {
			for (int order = 0; order < 3; ++order) {
				QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(count).arg(order))) << count << order;
			}
		}
	}

	void reassemblyBenchmark() {
		if (!qEnvironmentVariableIsSet("MUMBLE_BENCHMARK_REASSEMBLY")) {
			QSKIP("Opt in with MUMBLE_BENCHMARK_REASSEMBLY=1");
		}
		QFETCH(quint32, count);
		QFETCH(int, order);
		std::vector< quint32 > indices(count);
		std::iota(indices.begin(), indices.end(), 0u);
		if (order == 1) {
			std::reverse(indices.begin(), indices.end());
		} else if (order == 2) {
			std::mt19937 random(42);
			std::shuffle(indices.begin(), indices.end(), random);
		}
		ScreenShareReceiver receiver;
		Mumble::Protocol::VideoData data = packet(count);
		QBENCHMARK {
			for (quint32 index : indices) {
				data.fragmentIndex = index;
				receiver.handleVideoPacket(data);
			}
			++data.frameNumber;
		}
		QVERIFY(receiver.m_fragmentBuffer.at(1).empty());
	}
};

MUMBLE_CLIENT_TEST_MAIN(TestScreenShareReceiver)
#include "TestScreenShareReceiver.moc"
