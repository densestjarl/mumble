// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_BANDWIDTH_RECORD_H_
#define MUMBLE_BANDWIDTH_RECORD_H_

#include "Timer.h"

#include <QtCore/QMutex>
#include <QtCore/QtGlobal>

// Unfortunately, this needs to be "large enough" to hold
// enough frames to account for both short-term and
// long-term "maladjustments".

#define N_BANDWIDTH_SLOTS 360

struct BandwidthRecord {
	int iRecNum;
	qint64 iSum;
	Timer tFirst;
	Timer tIdleControl;
	int a_iBW[N_BANDWIDTH_SLOTS];
	Timer a_qtWhen[N_BANDWIDTH_SLOTS];
	mutable QMutex qmMutex;

	BandwidthRecord();
	bool addFrame(int size, int maxpersec);
	bool canAddFrame(int size, int maxpersec) const;
	bool frameAllowedUnlocked(int size, int maxpersec, qint64 &sum) const;
	int onlineSeconds() const;
	int idleSeconds() const;
	void resetIdleSeconds();
	int bandwidth() const;
};

// File relay budgets cap idle credit independently of voice's rolling history.
class FileBandwidthRecord {
	friend class TestBandwidthRecord;

public:
	static constexpr int MaximumFrameBytes = 1024 * 1024 + 16 + 128;
	static qint64 maximumBurst(int rate) { return static_cast< qint64 >(rate) + MaximumFrameBytes; }
	bool canAddFrame(int size, int rate) const;
	bool addFrame(int size, int rate);

private:
	qint64 available(int rate) const;
	mutable QMutex m_mutex;
	Timer m_refillClock;
	qint64 m_creditMicroBytes = 0;
	int m_rate                = 0;
};

#endif
