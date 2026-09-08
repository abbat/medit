#!/bin/sh
#
# Remove what a build, a test run or a documentation run leaves behind: the out
# of source cmake build directories, the files a package build drops into
# debian/, the interpreter's caches under tests/, and the doxygen output.
#
# Anything that is not build output — an editor's .vscode, local scratch files,
# uncommitted work — is left alone. To see what else is lying around, use
# "git clean -xdn".

set -eu

cd "$(dirname "$0")"

# Any directory holding a CMakeCache.txt is a build directory, whatever it is
# called: build, build2, build-gtk3, obj-x86_64-linux-gnu from dh_auto_configure.
for dir in */; do
    if [ -f "${dir}CMakeCache.txt" ]; then
        echo "removing ${dir%/}"
        rm -rf "$dir"
    fi
done

if [ -f CMakeCache.txt ]; then
    echo "warning: the source directory itself was configured; removing its cmake files" >&2
    rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake
fi

# What dpkg-buildpackage and debhelper leave in debian/
rm -rf debian/.debhelper debian/tmp debian/medit debian/medit-gtk2 debian/medit-gtk3
rm -f debian/files debian/debhelper-build-stamp
rm -f debian/*.substvars debian/*.debhelper.log debian/*.debhelper

# What a test run leaves: python caches beside every test.py, and beside the
# harness. They are gitignored, so they are invisible until an interpreter of a
# different version reads them.
find . -name __pycache__ -type d -prune -exec rm -rf {} +

# What "doxygen Doxyfile" writes; OUTPUT_DIRECTORY in Doxyfile names it.
rm -rf doxygen

# tests/coverage.py --output writes here when it is run by hand; in CI it stays
# inside the build directory.
rm -f coverage.info

echo "done"
