# medit

Second life for `medit` (also known as `mooedit`) text editor in modern linux distros.

* original website: https://mooedit.sourceforge.net
* last known release: 1.2.92 (2017-11-12)

## download / install

* [DEB](http://software.opensuse.org/download.html?project=home:antonbatenev:medit&package=medit)

## goals

* gtk-2 EOL at Dec 21, 2020 (version 2.24.33), so we have to migrate to gtk-3 (or gtk-4) in the future (see [migration 2to3](https://docs.gtk.org/gtk3/migrating-2to3.html)).
* also we can drop compatibility layer to older gtk2 and glib.
* current glib / gtk version matrix in modern linux distros for reference:

| distro       | glib            | gtk-2   | gtk-3             | gtk-4  |
|--------------|-----------------|---------|-------------------|--------|
| debian 11    | 2.66.8          | 2.24.33 | 3.24.24           | -      |
| debian 12    | 2.74.6          | 2.24.33 | 3.24.37           | 4.8.3  |
| ubuntu 20.04 | 2.64.2 (2.64.6) | 2.24.32 | 3.24.18 (3.24.20) | -      |
| ubuntu 22.04 | 2.72.1 (2.72.4) | 2.24.33 | 3.24.33           | 4.6.2  |
| fedora 37    | 2.74.7          | 2.24.33 | 3.24.38           | 4.8.3  |
| fedora 38    | 2.76.4          | 2.24.33 | 3.24.38           | 4.10.5 |
| minimum      | 2.64.2 (2.64.6) | 2.24.32 | 3.24.18 (3.24.20) | 4.6.2  |

* [glib changelog](https://gitlab.gnome.org/GNOME/glib/-/blob/main/NEWS)
* [gtk-2.24 changelog](https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-2-24/NEWS)
* [gtk-3.24 changelog](https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/NEWS)
* [gtk-4 changelog](https://gitlab.gnome.org/GNOME/gtk/-/blob/main/NEWS)

### sub-goals

I'm not sure we need it:

* keep compatibility with gtk-2
* migrate to mordern build system instead autotools (CMake?)
* migrate from glade to native code (remove python preprocessor from xml to c)

### remove

* keep compatibility with windows / darwin - I can not test & support it
* python bindings - EOL & removed with python2
