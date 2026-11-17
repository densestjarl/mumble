# Invite page

A static page that turns a server address into a link people can click anywhere:

```
https://<where this folder is hosted>/#voice.example.com:5000/Lobby?title=Friday%20night
```

Chat programs do not make `mumble://` links clickable, and a new user has no program for
them yet. This page offers the download for the visitor's system and a **Join now** button
that opens the `mumble://` link in the installed client. Opened without an address, it
shows a form that creates invite links.

## Link format

The fragment is a `mumble://` link without the scheme:

```
#[:<password>@]<host>[:<port>][/<channel>[/<subchannel>...]][?title=<name>]
```

The port defaults to 64738. A user name is not accepted, because each person chooses their
own in the client. Browsers do not send the fragment to the web server, so the host of the
page does not learn which servers people join. A password in the link is visible to
everyone who receives the link.

## Hosting

Copy `index.html`, `style.css`, `join.js` and `page.js` to any static web host; there is no
build step. The page loads no third-party code. Its only network request reads this
repository's public release list from the GitHub API to find the newest package. It prefers
stable releases, prefers the macOS disk image over the archive once releases carry one, and
falls back to the release page when the request fails.

`REPOSITORY` and `PACKAGES` in `join.js` must match `ReleaseUpdate.cpp` in the client.

## Tests

```sh
node --test web/join/join.test.js
```
