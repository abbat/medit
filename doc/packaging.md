# Packaging and releases

*For agents working in this tree. Read `AGENTS.md` first. Building is in
`doc/build.md`; this file is what happens to the result.*

## Debian package build (old distros)

The package targets **Debian 12 and 13, Ubuntu 22.04, 24.04 and 26.04** — Debian 11 and
Ubuntu 20.04 were dropped when their support ended. Each is covered once, and by the job
that adds the most:

| target | compiled by | why there |
|---|---|---|
| Ubuntu 22.04 | `build.yml` | the oldest gtk, glib, gcc and cmake of the five |
| Ubuntu 26.04 | `package.yml` | the newest of all four, and the packaging of the LTS most users are on |
| Debian 12 | `package.yml` | the oldest packaging; `debian/rules` is a gate there |
| Debian 13 | `ui.yml` | where the UI tests run anyway |
| Ubuntu 24.04 | nothing, on a push | between two ends that are both built; the release builds it by hand on OBS |

A package build costs two compiles, one per toolkit, which is why `package.yml` carries
two targets rather than five. `debian/rules`, `packages/PKGBUILD` and `packages/medit.spec`
all ask for `ENABLE_STRICT`, so every one of those builds is a gate rather than a smoke
test. What is worth doing by hand is the faster loop while *writing* a
packaging change, and the apt scenarios below, which CI does not reach:

```bash
docker build -t medit-deb - <<'EOF'
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq build-essential debhelper cmake \
    pkg-config intltool libgtk2.0-dev libgtk-3-dev libxml2-dev libxml2-utils \
    libjson-glib-dev libvte-2.91-dev libjpeg-dev
EOF
S=<scratch>                                                  # session scratch dir
git ls-files -z | tar --null -T - -czf $S/medit-src.tar.gz   # tracked files + local edits
docker run --rm -v $S:/w medit-deb bash -c 'set -o pipefail
  mkdir /build && cd /build && tar xzf /w/medit-src.tar.gz
  dpkg-buildpackage -us -uc -b -j8 2>&1 | tail -25'
```

The package builds medit **twice**, once per gtk version: `medit-gtk2` and
`medit-gtk3` carry the two builds and conflict with each other, and `medit` is an
arch-all metapackage depending on `medit-gtk3 | medit-gtk2`. So `debian/rules` runs
`dh_auto_configure`/`dh_auto_build`/`dh_auto_install` once per `--builddirectory`, and a
package build takes twice as long as a plain one. When changing the packaging, check
both the fresh install (`apt install medit` must pull gtk3, the first alternative), the
switch (`apt install medit-gtk2` must remove gtk3), and the upgrade from the old monolithic
`medit` (its `/usr/bin/medit` has to move to `medit-gtk3` without a file conflict —
that is what the `Breaks`/`Replaces: medit (<< 1.3.1)` are for). A fourth case is worth
one more run: a system already on `medit-gtk2` must **stay** there, because the installed
package still satisfies the alternative and apt does not reconsider the order.

Cache the image once (`docker build -t medit-u2004`); each fresh `apt-get install` costs
a few minutes. To collect **every** error in one pass instead of one per run, replace
`dpkg-buildpackage` with `cmake -S . -B b && cmake --build b -j8 -- -k 2>&1 | grep
error: | sort -u` — `-k` keeps make going after the first failing file.

The span the matrix covers is gcc 11 to gcc 15 and cmake 3.22 to cmake 4.2. The oldest
is Ubuntu 22.04; `cmake_minimum_required` still asks for 3.16, which is lower than
anything now tested and deliberately so — cmake 4 is the version that stops accepting
compatibility with anything before 3.5, and 3.16 is above that line.

What has actually broken on an old toolchain, none of it visible in a local build:

* **Symbols newer than the oldest target glib**, e.g. `G_REGEX_DEFAULT` (2.74) on Ubuntu
  22.04's 2.72. Use `(GRegexCompileFlags) 0`, as the rest of the tree does.
* **`g_object_ref` in C++** returns `gpointer` on older glib (no `typeof` magic), so
  assigning it to a typed field needs an explicit cast.
* **Unnamed parameters** in C function definitions (`static void f (Foo *x, gpointer)`) —
  legal in C++ and C23 only. gcc 9 errored with "parameter name omitted" while gcc 12
  accepted them silently at `-std=gnu17`. No compiler in the matrix rejects them any
  more, so this one is history rather than a live trap; write
  `G_GNUC_UNUSED gpointer data` anyway.

## Fedora and Arch packages

Two packaging trees live side by side: `debian/` (three packages, both gtk versions) and
`packages/` (everything else — `medit.spec`, `PKGBUILD`, and the OBS files). The spec and
the PKGBUILD build gtk-3 only. `PKGBUILD` builds from the GitHub tag tarball, so its
`sha256sums` follows the release and not the working tree; `medit.spec` expects
`medit-<version>.tar.bz2` in `SOURCES`, which on OBS the `_service` makes. To test either
against uncommitted work, tar the worktree with a `medit-<version>/` prefix.

```bash
docker build -t medit-f44 - <<'EOF'
FROM fedora:44
RUN dnf -y --setopt=install_weak_deps=False --disablerepo=fedora-cisco-openh264 install \
        rpm-build rpmdevtools cmake gcc gcc-c++ gtk3-devel glib2-devel libxml2-devel \
        gdk-pixbuf2-devel libICE-devel libSM-devel intltool gettext desktop-file-utils
EOF
```

`--disablerepo=fedora-cisco-openh264` is not optional: that repository is frequently
unreachable and a weak dependency drags it in, failing the image build.

Fedora compiles with **LTO and gcc 15**, which see things the Debian build cannot — in
`package.yml`'s rpm job, the only place in CI that builds this way:

* **`-Wodr`** catches two file-local structs sharing a name across translation units
  with different fields. They are only file-local by convention — C gives them external
  linkage — so LTO merges them. `RegexActionInfo`/`RegexFilterInfo` were renamed for
  this. An anonymous namespace would be the C++ answer, but it trades the warning for
  `-Wsubobject-linkage` as soon as an externally visible struct has such a member.
* **`-Wc++20-compat`** catches identifiers that became keywords: a variable named
  `requires` would stop compiling the day the project moves to C++20.

Two spec details that are easy to get wrong: `-DENABLE_INSTALL_HOOKS=OFF`, or
`gtk-update-icon-cache` runs inside `%{buildroot}` and ships a stale `icon-theme.cache`;
and `--no-warn-unused-cli`, which silences CMake's notice about the `*_RELEASE` and
Fortran flags `%cmake` passes unconditionally.

**CentOS is not a target and cannot be one.** CentOS Linux 8 died in 2021 and Stream 8
in 2024, their repositories survive only on vault.centos.org, and what is there is glib
2.56 / gtk 3.22 — below the floor this code needs. Stream 9 (gtk 3.24.31) and Stream 10
(3.24.43) would work if anyone asks.

## Cutting a release

**Before anything else, check that the distributions are still the right ones.** They
age between releases and nothing notices on its own. Compare what is claimed against
what is supported *today*, and fix both directions — drop what has reached end of life,
add what has been released since:

* `README.md` — the "DEB packages for …" line under **download**.
* `.github/workflows/build.yml` — the `deb` job's `image:`, which is the oldest target
  and only that, so an aged image there loses the low end of the range rather than one
  point of it.
* `.github/workflows/codeql.yml` — the runner and its dependency list.
* `.github/workflows/package.yml` — the `deb` matrix, which carries the newest target and
  the oldest packaging, and the Fedora release in the `rpm` job. **Build the targets no
  workflow covers by hand at release time**, on OBS: Ubuntu 24.04 is compiled nowhere on a
  push, and Ubuntu 22.04 and Debian 13 are compiled but not packaged.
* This file, "Debian package build (old distros)", which names the targets and the
  compiler span they cover.
* `debian/control`, `packages/medit.spec`, `packages/PKGBUILD` — dependency names
  occasionally move between packages across releases.

The same check applies to the actions the workflows pin. GitHub retires the Node
runtime under them on its own schedule, and the first sign is a warning in a green
job rather than a failure: `actions/checkout@v4` targets Node 20, which was
deprecated in September 2025, and jobs kept passing while being force-run on Node 24.
Read `uses:` in both workflows against the current major of each action
(`actions/checkout`, `github/codeql-action`); a bump costs nothing when the tree is
quiet and is a surprise when the runtime is finally withdrawn.

A dropped distribution usually takes a workaround with it: retiring Debian 11 removed
the whole `snapshot.debian.org` recipe its dead archive needed. A new one is worth a
container run before it goes in the matrix — Ubuntu 26.04 arrived with gcc 15 and cmake
4.2, two and three major versions ahead of anything the tree had been built with.

The version itself lives in seven places and they all have to move together. The
`version` job in `package.yml` compares all seven and fails if one is left behind, so
this is a list to work through rather than a thing to remember. `1.3.5` was cut like
this:

1. `CMakeLists.txt` — `MOO_MICRO_VERSION`. The comment above it says "keep in sync with
   debian/changelog", and that is the whole of the coupling: nothing derives one from
   the other.
2. `NEWS` — a dated `* === Released 1.3.4 ===` block at the **top**, prose, wrapped the
   way the file already is.
3. `debian/changelog` — a `medit (1.3.4) unstable; urgency=low` stanza at the top.
   `dch` is not used; the stanzas are written by hand, so mind the two-space indent,
   the blank line before the signature and the RFC 2822 date (`date -R`).
4. `packages/medit.spec` — `Version:` and a `%changelog` entry, newest first, dated
   `Day Mon DD YYYY`.
5. `packages/medit.dsc` — `Version:`.
6. `packages/PKGBUILD` — `pkgver`.
7. `README.md` — "current release of this fork", and the two tag examples in the
   paragraph about `git checkout`.

## `packages/` holds everything that is not the Debian tree

`packages/` is `medit.spec`, `PKGBUILD`, `medit.dsc` and `_service`. Three of those
four are the OBS package on `home:antonbatenev:medit`, so uploading a change there is
copying `_service`, `medit.spec` and `medit.dsc` out of this one directory. There used
to be a second spec under `rpm/` for Fedora alone; it said the same things twice and
drifted, and the one real difference between them is now a branch inside the single
file.

CI still builds the spec on `fedora:44` and the PKGBUILD on Arch — see the table above —
but the distributions the spec is *branched* for are checked only by OBS, which is where
they are built.

**OBS downloads nothing of its own.** Whatever files sit in the package directory are
copied into `SOURCES`, so a spec naming the tarball GitHub generates for a tag names a
file that is not there:

```
rpmuncompress -x /home/abuild/rpmbuild/SOURCES/medit-1.3.5.tar.gz
error: File ...: No such file or directory
```

`_service` is the answer: `tar_scm` fetches the branch and `recompress` makes
`medit-<version>.tar.bz2` out of what it fetched. The version comes from the tag —
`versionformat` takes the parent tag and the rewrite drops the leading `v` — so a
release is a tag and a trigger, and no file here names the tag.

`revision` has to be there all the same, naming the **branch**: the service defaults to
`master` and this repository's is `main`, and without it nothing is fetched at all.

```
COMMAND: ['git', 'reset', ..., 'master']
ERROR(128): fatal: ambiguous argument 'master': unknown revision
```

**Both services run on the server, and that is the whole shape of the file.** The
documented idiom for `obs_scm` is `<service name="tar" mode="buildtime"/>` after it,
which unpacks the `.obscpio` inside the build root — and a buildtime service is a build
dependency on the service package itself, which exists only for openSUSE. Every
repository in the project went `unresolvable` at once and nothing built:

```
nothing provides obs-service-tar, nothing provides obs-service-recompress,
nothing provides obs-service-set-version
have choice for wget needed by obs-service-download_files: wget1-wget wget2-wget
```

Server-side services need nothing in the build root. They leave files named
`_service:tar_scm:medit-<version>.tar.bz2`, and the worker strips that prefix when it
fills `SOURCES` (`bs_worker` in the OBS backend drops everything up to the last colon),
so the spec names the plain tarball. `tar_scm` rather than `obs_scm` for the same
reason: the `.obscpio` is smaller per revision, but unpacking it *is* the buildtime
service.

There is deliberately no `set_version`. It would write the tag's version into
`medit.spec` and `medit.dsc`, but those are kept correct in git and the `version` job
compares them; run on the server it would also leave a second `medit.spec` beside the
first.

**The repository is fetched twice, and the second time is not redundant.**
`debtransform` builds `debian/` out of what is in the *package directory* — either
files named `debian.*` or one archive called `debian.tar` — and never out of the
`debian/` that is inside the source tarball. Worse, it does not even run without one of
those: `build-recipe-dsc` decides by globbing, and with none there the build goes
straight to `dpkg-source`, which stops at a `.dsc` that has no `Files`:

```
DEB_TRANSFORM=
for f in $BUILD_ROOT$TOPDIR/SOURCES/debian.* ; do test -f $f && DEB_TRANSFORM=true ; done
```
```
/.build/build-recipe-dsc: BUILD/debian/control: No such file or directory
dpkg-source: error: missing critical source control field Files
```

So a second `tar_scm` fetches `subdir=debian` with `version=_none_`, which is the magic
value that keeps the version out of the name (`skip_versions` in the service's
`archive.py`): the file is `debian.tar` and not `debian-1.3.5.tar`, and `debian.tar` is
the name `debtransform` discovers by itself. Its paths have to start with `debian/`,
which is what the service produces, because `listtar` strips exactly that prefix.

**`recompress` takes `*.tar` and nothing narrower.** Its `--file` is not matched by the
service: the glob is expanded by the shell in `for i in $FILES`, against names that on
the server read `_service:tar_scm:medit-1.3.5.tar`, and `ls` is then called on the
result *quoted*, so it never globs. A pattern anchored at the package name matches
nothing there and the whole run fails:

```
ls: cannot access 'medit-*.tar': No such file or directory
Unknown file medit-*.tar
```

Compressing `debian.tar` along with the source costs nothing: `debtransform` accepts a
`.tar.bz2` debian archive, both to list it and to merge it. Note also that `recompress`
strips only the leading `_service:` and leaves `tar_scm:` in the name it writes; what
puts that right is the worker, whose substitution is greedy to the last colon.

Two details that removed worries rather than adding them: `debtransform` writes
`debian/source/format` itself when the format is quilt, so the `3.0 (native)` this tree
uses for its own builds does not leak into the OBS one; and the whole transform can be
run outside OBS — `obs-build` carries the script, and it was run here over the real
service output, producing `medit_1.3.5-1.dsc`, `medit_1.3.5.orig.tar.gz` and a
`debian.tar.gz` that `dpkg-source -x` unpacks.

The service chain can be run without OBS: it is packaged for Fedora as
`obs-service-tar_scm` and `obs-service-recompress`, and a `fedora:44` container against
GitHub reproduces what the server does. Two things that turned up there: `recompress`
**consumes** its input, leaving only the `.tar.bz2` in its outdir, and each service's
outdir is the next one's working directory, so running them all in one directory does
not reproduce the chain.

`osc` answers the "why is nothing building" question directly, without the web UI:

```bash
osc api "/source/home:antonbatenev:medit/medit?expand=1"   # did the service run, and what did it leave
osc api "/build/home:antonbatenev:medit/_result?package=medit&view=status"
```

The first shows `<serviceinfo code="succeeded"/>` and the generated files; the second
gives the `unresolvable` details above, which are what a red project usually is.

`packages/medit.spec` carries two things a plain Fedora spec would not: `Source0` is
the plain name the service leaves behind rather than a URL, and the handful of
`BuildRequires` openSUSE spells differently are branched (`gdk-pixbuf-devel`,
`gettext-tools`, `vte-devel`).

`ENABLE_STRICT` is on unconditionally, as it is in every other build here. On Fedora it
is also the only build of medit with LTO and so the only one that gets `-Wodr`, which
has caught real defects in this tree. The price is paid on OBS, which compiles against
distributions whose compilers are not pinned anywhere in this repository: a gcc nobody
here has run can fail a release build over a warning nobody has seen. That is a
deliberate trade — a warning in a log nobody reads is worth nothing — and it is the
first thing to look at when an OBS build goes red on a target that used to be green.

**`packages/medit.dsc` is not a `.dsc` as `dpkg-source` writes one.** It has no `Files` or
`Checksums`, and cannot: OBS extracts Debian sources with `dpkg-source -x`, which
verifies them, and a tarball the service rebuilds every run hashes differently every
run. What closes the gap is `debtransform`, which assembles the real source package at
build time and computes the checksums then, and which finds the one tarball in the
package by itself when there is no `Debtransform-Tar` line.

Two things `debtransform` does are worth knowing: it always produces a **non-native**
source package whatever the `Format:` line says, and it appends a Debian revision — the
deb comes out `1.3.5-1` where the hand-uploaded one was `1.3.5`, and the changelog gains
an entry saying `version number update by debtransform`. Everything about all three files — the source name, the prefix, the openSUSE
`BuildRequires`, the format `debtransform` settles on — **is checked by the first OBS
build after a change**, and that is the check to look at.

Then commit, merge to `main`, push, and tag:

```bash
git tag -a v1.3.4 -m "medit 1.3.4"
git push origin main v1.3.4
```

**The Arch checksum can only be filled in after the tag is pushed**, and it therefore
lands in a commit of its own, after the tag — the tarball GitHub generates for a tag
contains the PKGBUILD that would have to carry its own hash. Put
`sha256sums=('0000…')` in the release commit rather than `SKIP`, so that forgetting it
fails the build loudly. `git archive` does **not** reproduce GitHub's tarball (checked:
v1.3.2 hashes differently), so fetch the real one:

```bash
curl -sSL https://github.com/abbat/medit/archive/refs/tags/v1.3.4.tar.gz | sha256sum
```

---
