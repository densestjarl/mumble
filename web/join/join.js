// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

// Logic of the invite page. It has no dependencies and also loads in Node for the tests.
//
// An invite is the part of a mumble:// link after the scheme, carried in the page's fragment:
//   https://join.example/#voice.example:64738/Lobby?title=My%20community
// Browsers do not send the fragment to the web server, so the page can be hosted anywhere
// without the host learning which servers people join.

(function (root) {
	'use strict';

	const REPOSITORY = 'densestjarl/mumble';
	const RELEASES_PAGE = 'https://github.com/' + REPOSITORY + '/releases';
	const RELEASES_API = 'https://api.github.com/repos/' + REPOSITORY + '/releases?per_page=30';
	const DEFAULT_PORT = 64738;

	// Mirrors ReleaseUpdate::assetForPlatform in the client.
	const PACKAGES = {
		windows: { asset: 'Mumble-Windows-x64.exe', label: 'Windows', requires: 'Windows 10 or newer, 64-bit' },
		macos: { asset: 'Mumble-macOS-arm64.zip', label: 'macOS', requires: 'Apple Silicon, macOS 15 or newer' },
		linux: { asset: 'Mumble-Ubuntu-24.04-amd64.deb', label: 'Ubuntu', requires: 'Ubuntu 24.04, 64-bit' },
	};

	const INVITE = /^(?::([^@/?#]*)@)?(\[[0-9A-Fa-f:.]+\]|[^:/?#@[\]\s]+)(?::([0-9]{1,5}))?(\/[^?#]*)?(?:\?([^#]*))?$/;

	function normalizeHost(host) {
		try {
			// The URL parser validates the name and converts international names to ASCII.
			const url = new URL('https://' + host + '/');
			return url.username || url.password || url.port || url.pathname !== '/' ? null : url.hostname;
		} catch (error) {
			return null;
		}
	}

	// Returns { host, port, channel, title, password } or null when the text is not a usable invite.
	function parseInvite(fragment) {
		const text = String(fragment || '').replace(/^#/, '').replace(/^mumble:\/\//i, '');
		const match = INVITE.exec(text);
		if (!match) {
			return null;
		}
		const host = normalizeHost(match[2]);
		const port = match[3] === undefined ? DEFAULT_PORT : Number(match[3]);
		if (!host || port < 1 || port > 65535) {
			return null;
		}
		let channel, password, title;
		try {
			channel = decodeURIComponent(match[4] || '').replace(/^\/+|\/+$/g, '');
			password = decodeURIComponent(match[1] || '');
			title = (new URLSearchParams(match[5] || '').get('title') || '').trim().slice(0, 80);
		} catch (error) {
			return null;
		}
		return { host, port, channel, title, password };
	}

	function authority(invite) {
		return invite.host + (invite.port === DEFAULT_PORT ? '' : ':' + invite.port);
	}

	function channelPath(invite) {
		return invite.channel ? '/' + invite.channel.split('/').map(encodeURIComponent).join('/') : '';
	}

	// The link the installed client is registered for.
	function mumbleUrl(invite) {
		const query = new URLSearchParams();
		if (invite.title) {
			query.set('title', invite.title);
		}
		query.set('version', '1.2.0');
		const login = invite.password ? ':' + encodeURIComponent(invite.password) + '@' : '';
		// The client reads the query with QUrlQuery, which does not treat "+" as a space.
		return 'mumble://' + login + authority(invite) + channelPath(invite) + '/?'
			+ query.toString().replace(/\+/g, '%20');
	}

	// The https link to share. pageUrl is the address this page is served from.
	function inviteUrl(pageUrl, invite) {
		const login = invite.password ? ':' + encodeURIComponent(invite.password) + '@' : '';
		const title = invite.title ? '?title=' + encodeURIComponent(invite.title) : '';
		return String(pageUrl).replace(/[#?].*$/, '') + '#' + login + authority(invite) + channelPath(invite) + title;
	}

	function detectPlatform(userAgent) {
		const agent = String(userAgent || '');
		if (/Android|iPhone|iPad|iPod/i.test(agent)) {
			return 'mobile';
		}
		if (/Windows/i.test(agent)) {
			return 'windows';
		}
		if (/Macintosh|Mac OS X/i.test(agent)) {
			return 'macos';
		}
		if (/Linux|X11/i.test(agent)) {
			return 'linux';
		}
		return 'other';
	}

	function parseVersion(tag) {
		const match = /^v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$/.exec(String(tag));
		return match ? match.slice(1).map(Number) : null;
	}

	function newer(a, b) {
		for (let i = 0; i < 3; ++i) {
			if (a[i] !== b[i]) {
				return a[i] > b[i];
			}
		}
		return false;
	}

	// Picks the newest release that has the package, preferring stable releases over previews.
	// Like the client, it only accepts download links below this repository's release page.
	function pickDownload(releases, asset) {
		let best = null;
		for (const release of Array.isArray(releases) ? releases : []) {
			const version = release && release.draft === false ? parseVersion(release.tag_name) : null;
			if (!version) {
				continue;
			}
			const url = RELEASES_PAGE + '/download/' + release.tag_name + '/' + asset;
			const found = (Array.isArray(release.assets) ? release.assets : [])
				.some((item) => item && item.name === asset && item.state === 'uploaded'
					&& item.browser_download_url === url);
			const stable = release.prerelease === false;
			if (found && (!best || (stable && !best.stable) || (stable === best.stable && newer(version, best.version)))) {
				best = { version, stable, url, name: version.join('.') };
			}
		}
		return best;
	}

	const api = {
		DEFAULT_PORT, PACKAGES, RELEASES_API, RELEASES_PAGE,
		authority, detectPlatform, inviteUrl, mumbleUrl, parseInvite, pickDownload,
	};
	if (typeof module === 'object' && module.exports) {
		module.exports = api;
	} else {
		root.MumbleJoin = api;
	}
})(typeof self === 'undefined' ? this : self);
