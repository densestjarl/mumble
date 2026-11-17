// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "VersionCheck.h"

#include "MainWindow.h"
#include "NetworkConfig.h"
#include "ReleaseUpdate.h"
#include "ReleaseUpdateCache.h"
#include "ReleaseUpdateDownload.h"
#include "Version.h"
#include "Global.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include <QtWidgets/QMessageBox>
#include <memory>

namespace {

QString updateTitle() {
	return VersionCheck::tr("Mumble update");
}

/// Hands a downloaded and verified package to the platform's installer.
void installPackage(const QString &path, const QVersionNumber &version) {
	QWidget *window = Global::get().mw;
#if defined(Q_OS_WIN)
	Q_UNUSED(version);
	QMessageBox::information(window, updateTitle(),
							 VersionCheck::tr("Mumble will now close to install the update. Start it again when the "
											  "installer has finished."));
	// The installer replaces the files of the running client, so quit as soon as it has started.
	if (QProcess::startDetached(path, { QStringLiteral("/passive"), QStringLiteral("/norestart") })) {
		Global::get().mw->on_qaQuit_triggered();
	} else {
		QMessageBox::warning(
			window, updateTitle(),
			VersionCheck::tr("The installer could not be started. You can run it yourself:\n%1").arg(path));
	}
#elif defined(Q_OS_MACOS)
	Q_UNUSED(version);
	// Replacing the running application bundle is left to the user until builds are signed.
	QProcess::startDetached(QStringLiteral("/usr/bin/open"), { QStringLiteral("-R"), path });
	QMessageBox::information(window, updateTitle(),
							 VersionCheck::tr("The update was downloaded and verified. Quit Mumble, open the archive "
											  "and move the new Mumble into your Applications folder."));
#else
	// The package manager asks for the administrator password through the desktop's own prompt.
	auto *process = new QProcess(window);
	auto *busy    = new QProgressDialog(VersionCheck::tr("Installing the update…"), QString(), 0, 0, window);
	busy->setWindowTitle(updateTitle());
	busy->setWindowModality(Qt::WindowModal);
	busy->setMinimumDuration(0);
	const auto done = [process, busy, window, path, version](bool installed) {
		// Deletion waits for the outer event loop, so hide the dialog before the message box opens.
		busy->hide();
		busy->deleteLater();
		process->deleteLater();
		if (installed) {
			QMessageBox::information(
				window, updateTitle(),
				VersionCheck::tr("Mumble %1 is installed. Restart Mumble to use it.").arg(version.toString()));
		} else {
			QMessageBox::warning(window, updateTitle(),
								 VersionCheck::tr("The update could not be installed automatically. The verified "
												  "package is here:\n%1\n\nInstall it with:\nsudo apt install \"%1\"")
									 .arg(path));
		}
	};
	QObject::connect(process, &QProcess::finished, window, [done](int exitCode, QProcess::ExitStatus status) {
		done(status == QProcess::NormalExit && exitCode == 0);
	});
	QObject::connect(process, &QProcess::errorOccurred, window, [done](QProcess::ProcessError error) {
		if (error == QProcess::FailedToStart) {
			done(false);
		}
	});
	busy->show();
	process->start(QStringLiteral("pkexec"),
				   { QStringLiteral("/usr/bin/apt-get"), QStringLiteral("install"), QStringLiteral("--yes"), path });
#endif
}

} // namespace

VersionCheck::VersionCheck(bool autocheck, QObject *parent) : QObject(parent), m_autoCheck(autocheck) {
	// Queue the request so construction has finished before any completion path runs.
	QTimer::singleShot(0, this, &VersionCheck::performRequest);
}

void VersionCheck::performRequest() {
	const QString endpoint =
		MUMBLE_UPDATE_PRERELEASES ? QStringLiteral("releases?per_page=100") : QStringLiteral("releases/latest");
	QNetworkRequest request(
		QUrl(QStringLiteral("https://api.github.com/repos/%1/%2").arg(ReleaseUpdate::repository(), endpoint)));
	const QString cacheName =
		MUMBLE_UPDATE_PRERELEASES ? QStringLiteral("preview.json") : QStringLiteral("stable.json");
	const QString cachePath =
		QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/updates/") + cacheName;
	const auto cache =
		std::make_shared< ReleaseUpdate::Cache >(cachePath, request.url().toString(), MUMBLE_UPDATE_PRERELEASES);
	const qint64 now = QDateTime::currentSecsSinceEpoch();
	if (!cache->canRequest(m_autoCheck, now)) {
		if (!m_autoCheck) {
			QMessageBox::information(Global::get().mw, tr("Check for updates"),
									 tr("GitHub has temporarily limited update checks. Please try again later."));
		}
		deleteLater();
		return;
	}
	cache->recordAttempt(now);
	Network::prepareRequest(request);
	if (!cache->etag().isEmpty()) {
		request.setRawHeader("If-None-Match", cache->etag());
	}
	request.setRawHeader("Accept", "application/vnd.github+json");
	request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
	request.setTransferTimeout(15000);
	QNetworkReply *reply = Global::get().nam->get(request);
	// Bound both the total duration and response size, including a slowly trickling response.
	QTimer::singleShot(20000, reply, &QNetworkReply::abort);
	// The reply must clean itself up even if the window (and this checker) closes first.
	connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
	connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
		constexpr qsizetype maximumSize = ReleaseUpdate::MaximumResponseSize;
		m_response += reply->read(maximumSize + 1 - m_response.size());
		if (m_response.size() > maximumSize) {
			reply->abort();
		}
	});
	connect(reply, &QNetworkReply::finished, this, [this, reply, cache]() {
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (status == 403 || status == 429) {
			cache->rateLimited(QDateTime::currentSecsSinceEpoch(), reply->rawHeader("Retry-After"),
							   reply->rawHeader("X-RateLimit-Reset"));
		}
		const bool success = reply->error() == QNetworkReply::NoError
							 && cache->acceptResponse(status, m_response, reply->rawHeader("ETag"));
		if (!success) {
			if (!m_autoCheck) {
				QMessageBox::warning(Global::get().mw, tr("Check for updates"),
									 tr("Could not check GitHub for updates. Please try again later."));
			}
			deleteLater();
			return;
		}
		m_response = cache->body();
		showResult();
	});
}

void VersionCheck::showResult() {
	QString os = QString::fromLatin1(MUMBLE_TARGET_OS);
	if (os == QLatin1String("linux")) {
		os = QSysInfo::productType();
	}
	const QString asset =
		ReleaseUpdate::assetForPlatform(os, QString::fromLatin1(MUMBLE_TARGET_ARCH), QSysInfo::productVersion());
	const auto result = ReleaseUpdate::parse(m_response, asset, MUMBLE_UPDATE_PRERELEASES);
	const QVersionNumber current(MUMBLE_VERSION_MAJOR, MUMBLE_VERSION_MINOR, MUMBLE_VERSION_PATCH);
	if (!result.valid || result.release.version.isNull()) {
		if (!m_autoCheck) {
			QMessageBox::information(
				Global::get().mw, tr("Check for updates"),
				result.valid ? tr("No compatible versioned releases have been published for this update channel yet.")
							 : tr("GitHub returned an invalid update response. Please try again later."));
		}
	} else if (result.release.version <= current) {
		if (!m_autoCheck) {
			QMessageBox::information(Global::get().mw, tr("Check for updates"),
									 tr("Mumble %1 is up to date for this update channel.").arg(current.toString()));
		}
	} else {
		QMessageBox box(Global::get().mw);
		box.setWindowTitle(tr("Mumble update available"));
		box.setIcon(QMessageBox::Information);
		box.setTextFormat(Qt::PlainText);
		box.setText(tr("Mumble %1%2 is available. You are using %3.")
						.arg(result.release.version.toString(), result.release.preview ? tr(" (preview)") : QString(),
							 current.toString()));
		// Without a digest in the feed the package cannot be verified here, so leave it to the browser.
		const bool verifiable = result.release.verifiable();
		box.setInformativeText(
			result.release.download.isEmpty() ? tr("View the release notes for installation options on your platform.")
			: verifiable ? tr("Mumble can download and verify the new version for you. Your settings "
							  "will be preserved.")
						 : tr("Download the new version, then close Mumble and install it. Your "
							  "settings will be preserved."));
		auto *notes           = box.addButton(tr("Release notes"), QMessageBox::ActionRole);
		QPushButton *download = nullptr;
		if (!result.release.download.isEmpty()) {
			download =
				box.addButton(verifiable ? tr("Install update") : tr("Download update"), QMessageBox::AcceptRole);
		}
		box.addButton(tr("Later"), QMessageBox::RejectRole);
		box.exec();
		if (box.clickedButton() == notes) {
			QDesktopServices::openUrl(result.release.notes);
		} else if (download && box.clickedButton() == download) {
			if (verifiable) {
				downloadUpdate(result.release);
				return;
			}
			QDesktopServices::openUrl(result.release.download);
		}
	}
	deleteLater();
}

void VersionCheck::downloadUpdate(const ReleaseUpdate::Release &release) {
	const QString directory =
		QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/updates/packages");
	auto *download = new ReleaseUpdate::Download(Global::get().nam, release, directory, &Network::prepareRequest, this);
	auto *dialog = new QProgressDialog(tr("Downloading Mumble %1…").arg(release.version.toString()), tr("Cancel"), 0,
									   1000, Global::get().mw);
	dialog->setWindowTitle(updateTitle());
	dialog->setWindowModality(Qt::WindowModal);
	dialog->setMinimumDuration(0);
	dialog->setAutoClose(false);
	dialog->setAutoReset(false);
	dialog->setValue(0);
	connect(download, &ReleaseUpdate::Download::progress, dialog,
			[dialog](qint64 received, qint64 total) { dialog->setValue(static_cast< int >(received * 1000 / total)); });
	connect(dialog, &QProgressDialog::canceled, this, [this, download, dialog]() {
		download->cancel();
		dialog->deleteLater();
		deleteLater();
	});
	connect(download, &ReleaseUpdate::Download::failed, this,
			[this, dialog, release](ReleaseUpdate::Download::Error error) {
				dialog->hide();
				dialog->deleteLater();
				deleteLater();
				QMessageBox box(QMessageBox::Warning, updateTitle(),
								error == ReleaseUpdate::Download::Error::Mismatch
									? tr("The downloaded update did not match its published checksum and was deleted.")
									: tr("The update could not be downloaded."),
								QMessageBox::Close, Global::get().mw);
				auto *page = box.addButton(tr("Open download page"), QMessageBox::ActionRole);
				box.exec();
				if (box.clickedButton() == page) {
					QDesktopServices::openUrl(release.notes);
				}
			});
	connect(download, &ReleaseUpdate::Download::finished, this, [this, dialog, release](const QString &path) {
		dialog->hide();
		dialog->deleteLater();
		deleteLater();
		installPackage(path, release.version);
	});
	download->start();
}
