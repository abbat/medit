# The spec for building on OBS, which differs from rpm/medit.spec in three
# ways. Keep the two in step: the version job in .github/workflows/package.yml
# compares them, and the release procedure in AGENTS.md lists both.
#
# 1. The source. OBS downloads nothing of its own: whatever files sit in the
#    package directory are copied into SOURCES, so a Source0 naming the tarball
#    GitHub generates for a tag names a file that is not there, which is the
#    error this file exists to avoid:
#
#        rpmuncompress -x /home/abuild/rpmbuild/SOURCES/medit-1.3.5.tar.gz
#        error: File ...: No such file or directory
#
#    The tarball is made instead by the services in _service beside this file:
#    obs_scm fetches the tag and the buildtime services turn it into
#    medit-<version>.tar.bz2, unpacking a medit-<version>/ prefix -- which is
#    what %%autosetup expects with no arguments.
#
# 2. The build dependencies, which are named differently on openSUSE and on
#    the Fedora and RHEL family. Only the names that actually differ are
#    branched; the rest are the same package on both.
#
# 3. ENABLE_STRICT is off. rpm/medit.spec turns it on deliberately, being the
#    one build in this project with LTO and therefore with -Wodr, and accepts
#    that a new compiler can fail the package over a warning. OBS builds
#    against many distributions and compilers at once and none of them is
#    chosen here, so the same trade would mean a release failing to build for
#    a warning nobody has seen yet.

Name:           medit
Version:        1.3.5
Release:        1
Summary:        Useful programming and around-programming text editor
Group:          Productivity/Text/Editors

License:        LGPL-2.1-only
URL:            https://github.com/abbat/medit
Source0:        %{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

BuildRequires:  cmake
BuildRequires:  desktop-file-utils
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  glib2-devel
BuildRequires:  gtk3-devel
BuildRequires:  intltool
# the language server client
BuildRequires:  json-glib-devel
# the session management code talks to the X session manager directly
BuildRequires:  libICE-devel
BuildRequires:  libSM-devel
BuildRequires:  libxml2-devel

%if 0%{?suse_version}
BuildRequires:  gdk-pixbuf-devel
BuildRequires:  gettext-tools
# the terminal pane
BuildRequires:  vte-devel
%else
BuildRequires:  gdk-pixbuf2-devel
BuildRequires:  gettext
# the terminal pane
BuildRequires:  vte291-devel
%endif

Recommends:     ctags

%description
medit is a text editor with tabs, syntax highlighting, a file selector,
find in files, ctags navigation and user defined tools. This is a fork of
the editor Yevgen Muntyan stopped working on in 2017, ported to GTK+3.

%prep
%autosetup

%build
# the icon cache is updated by a file trigger, not by us; --no-warn-unused-cli
# silences the notice about the RELEASE and Fortran flags %%cmake always passes.
#
# %%cmake_build and %%cmake_install are the macros of current Fedora and current
# openSUSE. Leap 15.2 and older have %%cmake_install but not %%cmake_build; if
# the project builds for those, that is the next thing here to change.
%cmake --no-warn-unused-cli -DGTK_VERSION=3 -DENABLE_INSTALL_HOOKS=OFF \
    -DENABLE_TERMINAL=ON -DENABLE_LSP=ON
%cmake_build

%install
%cmake_install
%find_lang %{name}
%find_lang %{name}-gsv
cat %{name}-gsv.lang >> %{name}.lang

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/%{name}.desktop

%clean
rm -rf %{buildroot}

%files -f %{name}.lang
%defattr(-,root,root,-)
%license COPYING
%doc AUTHORS NEWS README.md THANKS
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/%{name}/
%{_mandir}/man1/%{name}.1*

%changelog
* Tue Sep 08 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.5-1
- The GTK+3 port's unreviewed blocks read through, and 19 defects fixed

* Sun Sep 06 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.4-1
- Language server client, and the syntax definitions of gtksourceview 5.20.0

* Sat Sep 05 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.3-1
- Terminal pane, built on vte-2.91

* Sat Sep 05 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.2-1
- Initial package
