// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

// Connects the invite logic in join.js to the page.

(function () {
	'use strict';

	const join = self.MumbleJoin;
	const $ = (id) => document.getElementById(id);

	// The packages are not signed yet, so each system asks for confirmation on first start.
	const WARNINGS = {
		windows: 'If Windows shows “Windows protected your PC”, choose More info, then Run anyway.',
		macos: 'Unzip it and move Mumble to Applications. If macOS blocks the first start, open '
			+ 'System Settings → Privacy & Security and choose Open Anyway.',
		linux: 'Install it with: sudo apt install ./Mumble-Ubuntu-24.04-amd64.deb',
	};

	function showInvite(invite) {
		$('invite-title').textContent = invite.title ? 'Join ' + invite.title : 'You are invited';
		document.title = invite.title ? 'Join ' + invite.title : document.title;
		$('invite-address').textContent = join.authority(invite);
		$('manual-host').textContent = invite.host;
		$('manual-port').textContent = String(invite.port);
		$('all-downloads').href = join.RELEASES_PAGE;

		const platform = join.detectPlatform(navigator.userAgent);
		const pack = join.PACKAGES[platform];
		if (platform === 'mobile') {
			$('install-intro').textContent = 'Mumble runs on Windows, macOS and Ubuntu. Open this link on a computer.';
		}
		if (pack) {
			$('download').textContent = 'Download for ' + pack.label;
			$('download-requires').textContent = 'Requires ' + pack.requires + '.';
			$('download-warning').textContent = WARNINGS[platform];
			// Until the release list arrives, the button leads to the release page.
			$('download').href = join.RELEASES_PAGE;
			fetch(join.RELEASES_API, { headers: { Accept: 'application/vnd.github+json' } })
				.then((response) => (response.ok ? response.json() : null))
				.then((releases) => {
					const download = join.pickDownload(releases, pack.asset);
					if (download) {
						$('download').href = download.url;
						$('download').textContent = 'Download for ' + pack.label + ' (' + download.name + ')';
					}
				})
				.catch(() => {});
		} else {
			$('download').textContent = 'See downloads';
			$('download').href = join.RELEASES_PAGE;
		}

		$('join').href = join.mumbleUrl(invite);
		$('join').addEventListener('click', () => {
			// A browser gives no signal when no program handles the link. If the page is
			// still in front shortly afterwards, Mumble most likely is not installed.
			setTimeout(() => {
				if (document.hasFocus()) {
					$('join-hint').hidden = false;
				}
			}, 2500);
		});
		$('invite').hidden = false;
	}

	function showBuilder(invalid) {
		$('builder-error').hidden = !invalid;
		const update = () => {
			const host = $('b-host').value.trim();
			const port = $('b-port').value.trim();
			const channel = $('b-channel').value.trim().split('/').map(encodeURIComponent).join('/');
			let invite = host ? join.parseInvite(host + (port ? ':' + port : '') + (channel ? '/' + channel : '')) : null;
			if (invite) {
				invite = { ...invite, title: $('b-title').value.trim(), password: $('b-password').value };
				$('b-link').value = join.inviteUrl(location.href, invite);
			}
			$('b-result').hidden = !invite;
			$('b-error').hidden = !host || Boolean(invite);
		};
		$('builder-form').addEventListener('input', update);
		$('builder-form').addEventListener('submit', (event) => event.preventDefault());
		$('b-copy').addEventListener('click', () => {
			$('b-link').select();
			navigator.clipboard.writeText($('b-link').value).then(() => {
				$('b-copy').textContent = 'Copied';
				setTimeout(() => ($('b-copy').textContent = 'Copy link'), 1500);
			}).catch(() => {});
		});
		$('builder').hidden = false;
	}

	function render() {
		$('invite').hidden = true;
		$('builder').hidden = true;
		const fragment = location.hash.replace(/^#/, '');
		const invite = join.parseInvite(fragment);
		if (invite) {
			showInvite(invite);
		} else {
			showBuilder(fragment !== '');
		}
	}

	// Opening another invite in the same tab only changes the fragment, so start over.
	window.addEventListener('hashchange', () => location.reload());
	render();
})();
