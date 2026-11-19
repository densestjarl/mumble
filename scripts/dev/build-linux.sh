#!/usr/bin/env bash
# Copyright The Mumble Developers. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file at the root of the Mumble source tree.

set -euo pipefail

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-${source_dir}/build-agent}"
if (( $# )); then shift; fi

for tool in cmake ninja pkg-config python3; do
	if ! command -v "$tool" >/dev/null; then
		echo "Missing ${tool}; see docs/dev/build-instructions/build_linux.md" >&2
		exit 1
	fi
done

# Refuse to move an initialized submodule away from local work.
submodule_status="$(git -C "$source_dir" submodule status --recursive)"
if grep -Eq '^[+U]' <<< "$submodule_status"; then
	echo "Submodule commits differ from the checkout; preserve that work before building." >&2
	exit 1
fi
git -C "$source_dir" submodule foreach --quiet --recursive \
	'test -z "$(git status --porcelain)" || { echo "Dirty submodule: $name" >&2; exit 1; }'
git -C "$source_dir" submodule update --init --recursive

cache_args=()
if command -v ccache >/dev/null; then
	export CCACHE_DIR="${CCACHE_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/mumble/ccache}"
	export CCACHE_BASEDIR="$source_dir"
	# Debug paths are mapped below, so hashing the build directory is unnecessary.
	export CCACHE_NOHASHDIR=true
	mkdir -p "$CCACHE_DIR"
	echo "Compiler cache: ${CCACHE_DIR}"
	cache_args=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
else
	echo "Optional: install ccache to reuse compilations across clean worktrees."
fi

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="$source_dir/cmake/mumble_dependency_provider.cmake" \
	-DBoost_NO_BOOST_CMAKE=ON -DCMAKE_UNITY_BUILD=OFF \
	-DCMAKE_C_FLAGS_DEBUG="-g -fdebug-prefix-map=${source_dir}=." \
	-DCMAKE_CXX_FLAGS_DEBUG="-g -fdebug-prefix-map=${source_dir}=." \
	-Dclient=ON -Dserver=ON -Dtests=ON -Dwarnings-as-errors=ON \
	-Ddatabase-sqlite-tests=ON -Ddatabase-mysql-tests=OFF \
	-Ddatabase-postgresql-tests=OFF -Donline-tests=OFF \
	-Doverlay-xcompile=OFF -Dlto=OFF "${cache_args[@]}" "$@"

# Avoid nproc on memory-constrained VMs: non-unity Debug C++ files are large.
cmake --build "$build_dir" --parallel "${BUILD_JOBS:-4}"
