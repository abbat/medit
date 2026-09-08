# The Debian recipe for OBS. Not a .dsc as dpkg-source writes one: no Files or
# Checksums, because the tarball is rebuilt on every service run and hashes
# differently every time. debtransform assembles the real source package at
# build time from this file, the source tarball and the debian.tar beside them.
# It always produces a non-native package, so the deb comes out 1.3.5-1.
# doc/packaging.md has the rest.
Format: 3.0 (quilt)
Source: medit
Version: 1.3.5
Binary: medit, medit-gtk2, medit-gtk3
Architecture: any all
Maintainer: Anton Batenev <antonbatenev@yandex.ru>
Homepage: https://github.com/abbat/medit
Standards-Version: 4.1.2
Vcs-Browser: https://github.com/abbat/medit
Vcs-Git: https://github.com/abbat/medit.git
Build-Depends: debhelper (>= 10), cmake, pkg-config, intltool, libjpeg62-turbo-dev | libjpeg-dev, libgtk2.0-dev, libgtk-3-dev, libvte-2.91-dev, libjson-glib-dev, libxml2-dev, libxml2-utils
Package-List:
 medit deb editors optional arch=all
 medit-gtk2 deb editors optional arch=any
 medit-gtk3 deb editors optional arch=any
