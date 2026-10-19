#!/usr/bin/env bash

set -e
set -x

# Bookworm's default CMake is older than the project's required version.
echo "deb http://deb.debian.org/debian bookworm-backports main" > /etc/apt/sources.list.d/mumble-backports.list
apt-get update

apt-get -y install \
	build-essential \
	ca-certificates \
	ccache \
	g++-multilib \
	git \
	libasound2-dev \
	libavahi-compat-libdnssd-dev \
	libavcodec-dev \
	libavdevice-dev \
	libavformat-dev \
	libavutil-dev \
	libboost-dev \
	libcap-dev \
	libgl-dev \
	libmsgsl-dev \
	libogg-dev \
	libopus-dev \
	libpipewire-0.3-dev \
	libpoco-dev \
	libpq-dev \
	libprotobuf-dev \
	libprotoc-dev \
	libqt6svg6-dev \
	libsndfile1-dev \
	libspeechd-dev \
	libspeexdsp-dev \
	libsqlite3-dev \
	libssl-dev \
	libswscale-dev \
	libxcb-xinerama0 \
	libxi-dev \
	libzeroc-ice-dev \
	ninja-build \
	nlohmann-json3-dev \
	pkgconf \
	protobuf-compiler \
	python3 \
	qt6-base-dev \
	qt6-base-dev-tools \
	qt6-l10n-tools \
	qt6-tools-dev \
	qt6-tools-dev-tools \
	qtchooser

apt-get -y install -t bookworm-backports cmake

# GCC 12 can emit bogus -Wrestrict warnings for std::string operations.
# Keep the warning visible without treating this compiler false positive as fatal.
echo "CXXFLAGS=-Wno-error=restrict" >> "$GITHUB_ENV"
