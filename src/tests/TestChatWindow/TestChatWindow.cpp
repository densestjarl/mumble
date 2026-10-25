// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "ChatWindow.h"

#include <QtTest/QtTest>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

class TestChatWindow : public QObject {
	Q_OBJECT

private slots:
	void escapePreservesChat() {
		ChatWindow window;
		QTextEdit input;
		window.layout()->addWidget(&input);
		QSignalSpy finished(&window, &QDialog::finished);
		window.show();
		input.setFocus();
		QTest::keyClicks(&input, "unsent draft");

		// Exercise the propagation from the input, as well as the window itself.
		QTest::keyClick(&input, Qt::Key_Escape);
		QTest::keyClick(&window, Qt::Key_Escape);
		QVERIFY(window.isVisible());
		QCOMPARE(finished.count(), 0);
		QCOMPARE(input.toPlainText(), QStringLiteral("unsent draft"));

		QTest::keyClicks(&input, " continued");
		QCOMPARE(input.toPlainText(), QStringLiteral("unsent draft continued"));
	}

	void explicitCloseNotifiesRestore() {
		ChatWindow window;
		QSignalSpy finished(&window, &QDialog::finished);
		window.show();
		const bool closed = window.close();
		QVERIFY(closed);
		QVERIFY(!window.isVisible());
		QCOMPARE(finished.count(), 1);
		QCOMPARE(finished.at(0).at(0).toInt(), static_cast< int >(QDialog::Rejected));
	}

	void popOutRestoresDockContents() {
		// Mirror MainWindow.ui: the chat dock holds a container with the input and send-file button.
		QMainWindow main;
		auto *logDock   = new QDockWidget(QStringLiteral("Log"), &main);
		auto *chatDock  = new QDockWidget(QStringLiteral("Chatbar"), &main);
		auto *log       = new QTextEdit;
		auto *chatBar   = new QWidget;
		auto *barLayout = new QHBoxLayout(chatBar);
		auto *input     = new QTextEdit;
		auto *sendFile  = new QToolButton;
		barLayout->addWidget(input);
		barLayout->addWidget(sendFile);
		logDock->setWidget(log);
		chatDock->setWidget(chatBar);
		main.addDockWidget(Qt::LeftDockWidgetArea, logDock);
		main.addDockWidget(Qt::BottomDockWidgetArea, chatDock);
		main.show();
		log->setPlainText(QStringLiteral("history"));
		input->setPlainText(QStringLiteral("unsent draft"));

		for (int cycle = 0; cycle < 3; ++cycle) {
			auto *window = new ChatWindow(&main);
			window->takeDockContents(logDock, chatDock);
			window->show();
			QCOMPARE(log->window(), window);
			QCOMPARE(input->window(), window);
			QCOMPARE(sendFile->window(), window);
			QVERIFY(sendFile->isVisible());

			window->restoreDockContents();
			delete window;
			QCOMPARE(logDock->widget(), log);
			QCOMPARE(chatDock->widget(), chatBar);
			QCOMPARE(sendFile->parentWidget(), chatBar);
			QVERIFY(input->isVisible());
			QVERIFY(sendFile->isVisible());
			QCOMPARE(log->toPlainText(), QStringLiteral("history"));
			QCOMPARE(input->toPlainText(), QStringLiteral("unsent draft"));
		}
	}
};

QTEST_MAIN(TestChatWindow)
#include "TestChatWindow.moc"
