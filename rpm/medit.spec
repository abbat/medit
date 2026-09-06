Name:           medit
Version:        1.3.3
Release:        1%{?dist}
Summary:        Useful programming and around-programming text editor

License:        LGPL-2.1-only
URL:            https://github.com/abbat/medit
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  desktop-file-utils
BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  gdk-pixbuf2-devel
BuildRequires:  gettext
BuildRequires:  glib2-devel
BuildRequires:  gtk3-devel
BuildRequires:  intltool
# the language server client
BuildRequires:  json-glib-devel
# the session management code talks to the X session manager directly
BuildRequires:  libICE-devel
BuildRequires:  libSM-devel
BuildRequires:  libxml2-devel
# the terminal pane
BuildRequires:  vte291-devel

Recommends:     ctags

%description
medit is a text editor with tabs, syntax highlighting, a file selector,
find in files, ctags navigation and user defined tools. This is a fork of
the editor Yevgen Muntyan stopped working on in 2017, ported to GTK+3.

%prep
%autosetup

%build
# the icon cache is updated by a file trigger, not by us; --no-warn-unused-cli
# silences the notice about the RELEASE and Fortran flags %%cmake always passes
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

%files -f %{name}.lang
%license COPYING
%doc AUTHORS NEWS README.md THANKS
%{_bindir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/%{name}/
%{_docdir}/%{name}/help/
%{_mandir}/man1/%{name}.1*

%changelog
* Sat Sep 05 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.3-1
- Terminal pane, built on vte-2.91

* Sat Sep 05 2026 Anton Batenev <antonbatenev@yandex.ru> - 1.3.2-1
- Initial package
