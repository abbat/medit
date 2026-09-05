# medit

Second life for `medit` (also known as `mooedit`) text editor in modern linux distros.

* original website: https://mooedit.sourceforge.net
* last upstream release: 1.2.92 (2017-11-12)
* current release of this fork: 1.3.1

## download / install

* [DEB](http://software.opensuse.org/download.html?project=home:antonbatenev:medit&package=medit)

## sources

This fork lives at <https://github.com/abbat/medit>:

```bash
git clone https://github.com/abbat/medit.git
cd medit
```

`main` carries the current state, and every release is tagged (`v1.3.0`,
`v1.2.92`, ...); `git checkout v1.3.0` gets the sources of a given release.
See [INSTALL](INSTALL) for how to build them.

The original sources up to 1.2.92, which this fork started from, are at
<https://sourceforge.net/projects/mooedit/files/medit/>.

## goals

* gtk-2 EOL at Dec 21, 2020 (version 2.24.33), so we have to migrate to gtk-3 (or gtk-4) in the future (see [migration 2to3](https://docs.gtk.org/gtk3/migrating-2to3.html)).
* also we can drop compatibility layer to older gtk2 and glib.
* current glib / gtk version matrix in modern linux distros for reference:

| distro       | glib            | gtk-2   | gtk-3             | gtk-4           |
|--------------|-----------------|---------|-------------------|-----------------|
| debian 12    | 2.74.6          | 2.24.33 | 3.24.37           | 4.8.3           |
| debian 13    | 2.84.4          | 2.24.33 | 3.24.49           | 4.18.6          |
| ubuntu 24.04 | 2.80.x          | 2.24.33 | 3.24.41           | 4.14.5          |
| ubuntu 26.04 | 2.86.x (2.88.x) | 2.24.33 | 3.24.50           | 4.22.4          |
| fedora 43    | 2.86.0 (2.86.5) | 2.24.33 | 3.24.51 (3.24.52) | 4.20.2 (4.20.4) |
| fedora 44    | 2.88.0 (2.88.3) | 2.24.33 | 3.24.52           | 4.22.1 (4.22.4) |
| minimum      | 2.64.2 (2.64.6) | 2.24.32 | 3.24.18 (3.24.20) | 4.6.2           |

* [glib changelog](https://gitlab.gnome.org/GNOME/glib/-/blob/main/NEWS)
* [gtk-2.24 changelog](https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-2-24/NEWS)
* [gtk-3.24 changelog](https://gitlab.gnome.org/GNOME/gtk/-/blob/gtk-3-24/NEWS)
* [gtk-4 changelog](https://gitlab.gnome.org/GNOME/gtk/-/blob/main/NEWS)

### sub-goals

I'm not sure we need it:

* keep compatibility with gtk-2
* migrate from glade to native code (remove python preprocessor from xml to c)
