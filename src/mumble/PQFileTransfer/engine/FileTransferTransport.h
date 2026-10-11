// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MUMBLE_PQFILETRANSFER_ENGINE_FILETRANSFERTRANSPORT_H_
#define MUMBLE_MUMBLE_PQFILETRANSFER_ENGINE_FILETRANSFERTRANSPORT_H_

#include <QByteArray>
#include <QList>

#include <memory>
#include <optional>

namespace PQFT::Transport {

// The handler owns connection-ID validation, accounting and queued delivery.
// Keep one production implementation; these helpers only bind its lifetime.
template< typename Handler >
bool queueControl(Handler *handler, unsigned int targetSession, const QByteArray &payload, int connectionID) {
	return handler && handler->queueFileTransferControl(targetSession, payload, connectionID);
}

template< typename Handler >
bool queueChunk(Handler *handler, const QList< unsigned int > &targets, const QByteArray &transferId, quint64 index,
				quint64 total, const QByteArray &data, int connectionID) {
	return handler && handler->queueFileData(targets, transferId, index, total, data, connectionID);
}

template< typename Handler >
bool queueControl(const std::weak_ptr< Handler > &connection, unsigned int targetSession, const QByteArray &payload,
				  int connectionID) {
	const auto handler = connection.lock();
	return queueControl(handler.get(), targetSession, payload, connectionID);
}

template< typename Handler >
bool queueChunk(const std::weak_ptr< Handler > &connection, const QList< unsigned int > &targets,
				const QByteArray &transferId, quint64 index, quint64 total, const QByteArray &data, int connectionID) {
	const auto handler = connection.lock();
	return queueChunk(handler.get(), targets, transferId, index, total, data, connectionID);
}

} // namespace PQFT::Transport

#endif // MUMBLE_MUMBLE_PQFILETRANSFER_ENGINE_FILETRANSFERTRANSPORT_H_
