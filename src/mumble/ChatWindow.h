// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_CHATWINDOW_H_
#define MUMBLE_MUMBLE_CHATWINDOW_H_

#include <QtCore/QPointer>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QVBoxLayout>

/// Separate window that temporarily hosts the chat log and chat bar.
///
/// Keeps normal chat keyboard behavior while retaining QDialog's explicit close notification.
class ChatWindow : public QDialog {
public:
	explicit ChatWindow(QWidget *parent = nullptr) : QDialog(parent, Qt::Window), m_layout(new QVBoxLayout(this)) {}

	/// Move each dock's content widget into this window. The widgets themselves are moved, not
	/// copied, so history, drafts and signal connections survive.
	void takeDockContents(QDockWidget *logDock, QDockWidget *chatDock) {
		m_logDock  = logDock;
		m_chatDock = chatDock;
		m_log      = logDock->widget();
		m_chatBar  = chatDock->widget();
		m_layout->addWidget(m_log, 1);
		m_layout->addWidget(m_chatBar);
		m_log->show();
		m_chatBar->show();
	}

	/// Put the widgets back as the content of the docks they came from.
	void restoreDockContents() {
		if (m_log && m_logDock) {
			m_layout->removeWidget(m_log);
			m_logDock->setWidget(m_log);
			m_log->show();
		}
		if (m_chatBar && m_chatDock) {
			m_layout->removeWidget(m_chatBar);
			m_chatDock->setWidget(m_chatBar);
			m_chatBar->show();
		}
		m_log     = nullptr;
		m_chatBar = nullptr;
	}

protected:
	void keyPressEvent(QKeyEvent *event) override {
		// Escape in the chat input must not reject the dialog and return chat to its docks.
		if (event->key() == Qt::Key_Escape) {
			event->accept();
			return;
		}
		QDialog::keyPressEvent(event);
	}

private:
	QVBoxLayout *m_layout;
	QPointer< QDockWidget > m_logDock, m_chatDock;
	QPointer< QWidget > m_log, m_chatBar;
};

#endif // MUMBLE_MUMBLE_CHATWINDOW_H_
