# The spec medit is built from, on OBS and by the rpm job in
# .github/workflows/package.yml. Source0 is the tarball _service leaves in the
# package rather than a URL, and the BuildRequires openSUSE spells differently
# are branched. doc/packaging.md has the rest, including why ENABLE_STRICT is on for a
# builder whose compilers are pinned nowhere here, and which other six files
# have to carry the same version.

Name:           medit
Version:        1.3.6
Release:        1%{?dist}
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
# --no-warn-unused-cli silences the notice about the flags %%cmake always
# passes. %%cmake_build is current Fedora and openSUSE, but not Leap 15.2.
%cmake --no-warn-unused-cli -DGTK_VERSION=3 -DENABLE_INSTALL_HOOKS=OFF \
    -DENABLE_TERMINAL=ON -DENABLE_LSP=ON -DENABLE_STRICT=ON
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
* Wed Sep 09 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.6-1
- Four defects the new tests found, in the tab strip, the Document menu and
  Preferences / Tools

* Tue Sep 08 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.5-1
- The GTK+3 port's unreviewed blocks read through, and 19 defects fixed

* Sun Sep 06 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.4-1
- Language server client, and the syntax definitions of gtksourceview 5.20.0

* Sat Sep 05 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.3-1
- Terminal pane, built on vte-2.91

* Sat Sep 05 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.2-1
- Initial package
