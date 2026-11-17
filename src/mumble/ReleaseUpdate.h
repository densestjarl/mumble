// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_RELEASEUPDATE_H_
#define MUMBLE_MUMBLE_RELEASEUPDATE_H_

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QVersionNumber>

namespace ReleaseUpdate {

/// Larger packages are not downloaded by the client.
constexpr qint64 MaximumDownloadSize = Q_INT64_C(1024) * 1024 * 1024;

struct Release {
	QVersionNumber version;
	QUrl notes;
	QUrl download;
	/// SHA-256 and size of the download as reported by GitHub. Empty and 0 when the feed lacks them,
	/// in which case the package cannot be verified and must not be installed from within the client.
	QByteArray sha256;
	qint64 size  = 0;
	bool preview = false;

	bool verifiable() const { return !download.isEmpty() && sha256.size() == 32 && size > 0; }
};

struct Result {
	bool valid = false;
	Release release;
};

QString repository();
QString assetForPlatform(const QString &os, const QString &architecture, const QString &osVersion);
// Select by numeric version, independent of publication order. An empty asset name means notes only.
Result parse(const QByteArray &json, const QString &assetName, bool includePreviews);

} // namespace ReleaseUpdate

#endif
