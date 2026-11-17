// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "ReleaseUpdateDownload.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <utility>

namespace ReleaseUpdate {

Download::Download(QNetworkAccessManager *manager, const Release &release, const QString &directory,
				   std::function< void(QNetworkRequest &) > prepare, QObject *parent)
	: QObject(parent), m_manager(manager), m_release(release),
	  m_path(QDir(directory).filePath(QFileInfo(release.download.path()).fileName())), m_prepare(std::move(prepare)) {
}

Download::~Download() {
	cancel();
}

void Download::start() {
	// An earlier package under the final name was verified against a different release.
	QFile::remove(m_path);
	m_file.setFileName(m_path + QLatin1String(".part"));
	if (!m_release.verifiable() || !QDir().mkpath(QFileInfo(m_path).absolutePath())
		|| !m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		// Report asynchronously so that callers can connect after start().
		QMetaObject::invokeMethod(this, [this]() { fail(Error::Storage); }, Qt::QueuedConnection);
		return;
	}
	QNetworkRequest request(m_release.download);
	if (m_prepare) {
		m_prepare(request);
	}
	// The download is served from a storage host that GitHub redirects to. Its content is
	// checked against the digest below, so the redirect target does not need to be trusted.
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	// Abort a stalled transfer; a slow but progressing one may take as long as it needs.
	request.setTransferTimeout(30000);
	m_reply = m_manager->get(request);
	connect(m_reply, &QNetworkReply::readyRead, this, &Download::read);
	connect(m_reply, &QNetworkReply::finished, this, &Download::complete);
}

void Download::cancel() {
	if (m_done) {
		return;
	}
	m_done = true;
	if (m_reply) {
		m_reply->disconnect(this);
		m_reply->abort();
		m_reply->deleteLater();
	}
	m_file.close();
	m_file.remove();
}

void Download::read() {
	while (m_reply->bytesAvailable() > 0) {
		const QByteArray block = m_reply->read(256 * 1024);
		m_received += block.size();
		if (m_received > m_release.size) {
			fail(Error::Mismatch);
			return;
		}
		m_hash.addData(block);
		if (m_file.write(block) != block.size()) {
			fail(Error::Storage);
			return;
		}
	}
	emit progress(m_received, m_release.size);
}

void Download::complete() {
	if (m_reply->error() != QNetworkReply::NoError) {
		fail(Error::Network);
		return;
	}
	read();
	if (m_done) {
		return;
	}
	if (m_received != m_release.size || m_hash.result() != m_release.sha256) {
		fail(Error::Mismatch);
		return;
	}
	if (!m_file.flush() || !m_file.rename(m_path)) {
		fail(Error::Storage);
		return;
	}
	m_file.close();
	m_done = true;
	m_reply->deleteLater();
	emit finished(m_path);
}

void Download::fail(Error error) {
	cancel();
	emit failed(error);
}

} // namespace ReleaseUpdate
