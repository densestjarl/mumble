// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_RELEASEUPDATEDOWNLOAD_H_
#define MUMBLE_MUMBLE_RELEASEUPDATEDOWNLOAD_H_

#include "ReleaseUpdate.h"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace ReleaseUpdate {

/// Downloads the package of a release into a directory and verifies its size and SHA-256.
/// Only a verified package is ever present under its final name.
class Download : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(Download)

public:
	enum class Error { Network, Storage, Mismatch };

	/// @param prepare Optionally adjusts the request (user agent, proxy settings) before it is sent.
	Download(QNetworkAccessManager *manager, const Release &release, const QString &directory,
			 std::function< void(QNetworkRequest &) > prepare = {}, QObject *parent = nullptr);
	~Download() override;

	void start();
	/// Stops the transfer and removes the partial file. No further signal is emitted.
	void cancel();

signals:
	void progress(qint64 received, qint64 total);
	void finished(const QString &path);
	void failed(ReleaseUpdate::Download::Error error);

private:
	void read();
	void complete();
	void fail(Error error);

	QNetworkAccessManager *m_manager;
	Release m_release;
	QString m_path;
	std::function< void(QNetworkRequest &) > m_prepare;
	QPointer< QNetworkReply > m_reply;
	QFile m_file;
	QCryptographicHash m_hash{ QCryptographicHash::Sha256 };
	qint64 m_received = 0;
	bool m_done       = false;
};

} // namespace ReleaseUpdate

#endif
