// Copyright The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

'use strict';

const assert = require('node:assert/strict');
const test = require('node:test');
const join = require('./join.js');

const base = 'https://github.com/densestjarl/mumble/releases/';
function release(version, prerelease = true, asset = 'Mumble-Windows-x64.exe') {
	const tag = 'v' + version;
	return {
		tag_name: tag,
		draft: false,
		prerelease,
		assets: [{ name: asset, state: 'uploaded', browser_download_url: base + 'download/' + tag + '/' + asset }],
	};
}

test('parses host, port, channel and title', () => {
	assert.deepEqual(join.parseInvite('#voice.example:5000/Lobby/Team%20A?title=Friday%20night'),
		{ host: 'voice.example', port: 5000, channel: 'Lobby/Team A', title: 'Friday night', password: '' });
	assert.deepEqual(join.parseInvite('voice.example'),
		{ host: 'voice.example', port: 64738, channel: '', title: '', password: '' });
	assert.equal(join.parseInvite('mumble://Voice.Example/').host, 'voice.example');
	assert.equal(join.parseInvite('203.0.113.7:443').port, 443);
	assert.equal(join.parseInvite('[2001:db8::1]:5000').host, '[2001:db8::1]');
	assert.equal(join.parseInvite(':s3cret%21@voice.example').password, 's3cret!');
	assert.equal(join.parseInvite('bücher.example').host, 'xn--bcher-kva.example');
});

test('rejects text that is not a server address', () => {
	for (const text of ['', '#', 'voice.example:0', 'voice.example:70000', 'voice.example:port', 'user@voice.example',
		'user:pw@voice.example', 'voice example', 'javascript:alert(1)', '//evil.example', 'a.example/%zz',
		'voice.example\\@evil.example', 'voice.example:1:2', '<script>']) {
		assert.equal(join.parseInvite(text), null, text);
	}
});

test('builds the link the client is registered for', () => {
	assert.equal(join.mumbleUrl(join.parseInvite('voice.example')), 'mumble://voice.example/?version=1.2.0');
	assert.equal(join.mumbleUrl(join.parseInvite(':p w@voice.example:5000/Lobby/Team%20A?title=Friday+night%20%26+more')),
		'mumble://:p%20w@voice.example:5000/Lobby/Team%20A/?title=Friday%20night%20%26%20more&version=1.2.0');
});

test('an invite survives being shared and opened again', () => {
	const invite = { host: 'voice.example', port: 5000, channel: 'Lobby/A&B #1', title: 'Fünf?', password: 'a/b@c' };
	const shared = join.inviteUrl('https://join.example/index.html?x=1#old', invite);
	assert.ok(shared.startsWith('https://join.example/index.html#'));
	assert.deepEqual(join.parseInvite(new URL(shared).hash), invite);
	assert.equal(join.inviteUrl('https://join.example/', join.parseInvite('voice.example')),
		'https://join.example/#voice.example');
});

test('detects the platform', () => {
	assert.equal(join.detectPlatform('Mozilla/5.0 (Windows NT 10.0; Win64; x64)'), 'windows');
	assert.equal(join.detectPlatform('Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)'), 'macos');
	assert.equal(join.detectPlatform('Mozilla/5.0 (X11; Ubuntu; Linux x86_64)'), 'linux');
	assert.equal(join.detectPlatform('Mozilla/5.0 (Linux; Android 15; Pixel 9)'), 'mobile');
	assert.equal(join.detectPlatform('Mozilla/5.0 (iPhone; CPU iPhone OS 18_0 like Mac OS X)'), 'mobile');
	assert.equal(join.detectPlatform(undefined), 'other');
});

test('picks the newest complete release and prefers stable ones', () => {
	const asset = 'Mumble-Windows-x64.exe';
	assert.equal(join.pickDownload([release('1.7.9'), release('1.7.100'), release('1.7.11')], asset).name, '1.7.100');
	assert.equal(join.pickDownload([release('1.7.100'), release('1.7.50', false), release('1.7.40', false)], asset).name,
		'1.7.50');
	assert.equal(join.pickDownload([release('1.7.100', true, 'Mumble-macOS-arm64.zip'), release('1.7.9')], asset).name,
		'1.7.9');
	assert.equal(join.pickDownload([release('1.7.9')], asset).url, base + 'download/v1.7.9/' + asset);
});

test('ignores drafts, odd tags and foreign download links', () => {
	const asset = 'Mumble-Windows-x64.exe';
	const draft = { ...release('1.7.9'), draft: true };
	const legacy = { ...release('1.7.9'), tag_name: 'master-preview' };
	const foreign = release('1.7.9');
	foreign.assets[0].browser_download_url = 'https://evil.example/' + asset;
	const pending = release('1.7.9');
	pending.assets[0].state = 'new';
	for (const feed of [[draft], [legacy], [foreign], [pending], [], null, { message: 'rate limit' }, [null]]) {
		assert.equal(join.pickDownload(feed, asset), null);
	}
});
