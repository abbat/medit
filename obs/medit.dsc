# The Debian recipe for OBS. Not a .dsc as dpkg-source would write one: there
# are no Files or Checksums fields here, and there cannot be, because the
# tarball is made by the services in _service on every run and its checksums
# are different every time.
#
# OBS handles that with debtransform, which assembles the real source package
# at build time and computes the checksums then. With no Debtransform-Tar line
# it finds the one tarball in the package by itself, which is what the service
# leaves there:
#
#     No DEBTRANSFORM-TAR line in the .dsc file.
#     Attempting automatic discovery of a suitable source archive.
#
# Two things debtransform does that are worth knowing before the first build.
# It always produces a *non-native* source package, whatever this file says,
# and it appends a Debian revision to the version: the deb that comes out is
# 1.3.5-1 where the hand-uploaded one was 1.3.5. And it re-sets Format, so the
# 3.0 (quilt) below is a request for its 3.0 path rather than its 1.0 one --
# the alternative, and what it falls back to, is 1.0 with a unitary diff.
#
# Version is kept in step with the other files by the version job, and
# set_version overwrites it at build time from the tag as well.
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
