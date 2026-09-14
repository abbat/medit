"""Sanitizer environment and log analysis.

A UI test has two verdicts, not one. The first is whether the scenario did what
it was supposed to do; the second is what the sanitizers saw while it did it,
which is the more interesting half -- a leak or a use-after-free in a dialog is
invisible to a test that only checks that the dialog appeared.

Both have to pass for the test to pass. The unit tests are judged the same way,
through lib/unit.py: they run inside the same sanitized medit, so there is no
reason for the two halves of the suite to report what the runtime saw
differently.

**Where a finding is charged matters as much as that it happened.** LSan blames a
leak on the nearest frame that is not in a system library, which in a GUI means
pango's font and shaping caches are reported against whichever of our functions
first asked pango to lay text out -- 289 of the 633 records a clean startup and
exit used to produce, the largest group of them against a function that frees
everything it allocates. So a record is classified here by the frame that
actually asked for the memory (_site() below), and a leak allocated inside a
library does not fail a test. Nothing else is filtered: an ASan error or a UBSan
report always does, because both are instrumented in our own code and a library
frame in one of those stacks is our misuse of that library.
"""

import glob
import json
import os
import re


ASAN_PREFIX = "asan"
UBSAN_PREFIX = "ubsan"

# tests/lib/sanitizer.py -> the tree this was built from. Debug info records the
# path the compiler saw, so a frame is ours when its source file is under here.
SOURCE_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VENDOR_ROOT = os.path.join(SOURCE_ROOT, "src", "vendor")

# What the runtime exits with when it reported something, rather than the 1 it
# uses by default -- which a test cannot tell from the program's own failure,
# and which says nothing about what was reported. Whether the report matters is
# decided here, by reading the logs; the status only says there is one to read.
EXIT_CODE = 23

# "==1234==ERROR: AddressSanitizer: heap-use-after-free on address 0x..."
_ERROR = re.compile(r"==\d+==ERROR: (AddressSanitizer|LeakSanitizer): (.+)")
# "Direct leak of 40 byte(s) in 1 object(s) allocated from:"
_LEAK = re.compile(r"^(Direct|Indirect) leak of (\d+) byte\(s\) in (\d+) object\(s\)")
# Two shapes, with debug info and without:
#   "    #0 0x5555 in moo_foo /home/x/medit/src/mooutils/moofoo.cpp:42:9"
#   "    #1 0x7f2b in pango_shape (/usr/lib/libpango-1.0.so.0+0x2f1a3)"
_FRAME = re.compile(r"^\s*#(\d+) 0x[0-9a-f]+\s+(?:in (\S+)\s+)?(.*?)\s*$")
# "src/mooedit/mooedit.cpp:12:5: runtime error: signed integer overflow: ..."
_RUNTIME = re.compile(r"^(.*?):(\d+):(?:(\d+):)? runtime error: (.+)$")
# A source location at the end of a frame: path, line, and sometimes a column.
_LOCATION = re.compile(r"^(.*?):(\d+)(?::\d+)?$")

# Frames that allocate on behalf of whoever called them: libc's entry points,
# the interceptors ASan puts in front of them, and glib's memory and container
# primitives. The allocation site is the first frame past these, which is what
# tells "our code built this hash table" from "pango did".
#
# Over-listing is safe in a way that under-listing is not. Skipping a helper a
# library called only moves the answer to the library frame below it, while
# skipping too little leaves every g_malloc()ed byte in the tree charged to
# glib and the whole classification says "library" for everything.
_WRAPPER_NAMES = frozenset((
    "malloc", "calloc", "realloc", "reallocarray", "valloc", "pvalloc",
    "memalign", "aligned_alloc", "posix_memalign", "strdup", "strndup",
    "operator new", "operator new[]", "wcsdup",
))

_WRAPPER_PREFIXES = (
    "__interceptor_", "__libc_", "_int_malloc", "operator new",
    # glibc's own allocating helpers, which carry debug info and would
    # otherwise be the answer for everything printed through them.
    "__vasprintf", "__asprintf", "vasprintf", "asprintf", "__strdup",
    # glib's memory primitives.
    "g_malloc", "g_realloc", "g_try_", "g_slice_", "g_memdup", "g_new",
    "g_aligned_alloc", "g_steal_", "g_rc_box_", "g_atomic_rc_box_",
    # Strings, which glib allocates one way or another in half its API.
    "g_str", "g_ascii_str", "g_utf8_str", "g_utf16_", "g_ucs4_", "g_convert",
    "g_locale_", "g_filename_", "g_build_", "g_path_get", "g_uri_",
    "g_markup_escape", "g_shell_",
    # Containers, which hold what the caller put in them.
    "g_array_", "g_ptr_array_", "g_byte_array_", "g_bytes_", "g_list_",
    "g_slist_", "g_queue_", "g_hash_table_", "g_tree_", "g_node_",
    "g_sequence_", "g_string_", "g_variant_", "g_key_file_", "g_error_new",
    "g_set_error", "g_datalist_", "g_dataset_", "g_quark_",
    # The object machinery. A widget gtk creates for itself lands on the gtk
    # frame below these; one our code asks for lands on ours.
    "g_object_new", "g_type_create_instance", "g_boxed_copy", "g_value_",
    "g_param_spec_", "g_closure_", "g_signal_connect", "g_cclosure_",
)


def clean_logs(log_dir):
    """Remove what an earlier run of this test left in its log directory.

    Each runtime appends its pid to the file it writes, so nothing here is
    overwritten on its own and a report from yesterday would be read as this
    run's.
    """
    os.makedirs(log_dir, exist_ok=True)

    for pattern in ("asan.*", "ubsan.*", "*.log", "*.json", "*.png"):
        for stale in glob.glob(os.path.join(log_dir, pattern)):
            os.unlink(stale)


def options(log_dir, suppressions=None, leak_check=False):
    """The environment the sanitizer runtimes read.

    log_path makes each runtime write to a file of its own instead of stderr,
    where it would be mixed in with gtk warnings and with medit's own output.
    Each runtime appends its pid to the name.

    Leak checking is off unless asked for -- in a UI test, which starts gtk; the
    unit tests run before gtk_init() and have it on always, where a clean exit
    reports nothing at all. What a GUI adds is pango's and fontconfig's caches,
    633 records and 121 KB of them at a clean exit, and they are filtered by
    where they were allocated rather than suppressed by name:

        ctest -R about_dialog       # ASan and UBSan
        UI_TEST_LEAK_CHECK=1 ctest -R about_dialog
    """
    asan = [
        "log_path=" + os.path.join(log_dir, ASAN_PREFIX),
        "detect_leaks=%d" % (1 if leak_check else 0),
        "abort_on_error=0",
        "print_suppressions=0",
        # gtk loads its modules with dlopen and never unloads them; without this
        # every symbol in a still-loaded module is reported as an interceptor
        # mismatch on some toolchains.
        "detect_odr_violation=0",
    ]

    if leak_check:
        # LSan records allocation stacks the fast way by default, and the system
        # libraries are built without frame pointers, so every stack comes out
        # two frames deep -- which is exactly the information the classification
        # below needs. It costs a visibly slower run, which is why it is here
        # and not in the options every test gets.
        asan += ["fast_unwind_on_malloc=0", "malloc_context_size=25"]

    asan.append("exitcode=%d" % EXIT_CODE)

    ubsan = [
        "log_path=" + os.path.join(log_dir, UBSAN_PREFIX),
        "print_stacktrace=1",
        # Report every site rather than stopping at the first one: one run of
        # one dialog is expensive, and we want everything it touched.
        "halt_on_error=0",
    ]
    lsan = ["print_suppressions=0"]

    if suppressions and os.path.exists(suppressions):
        lsan.append("suppressions=" + suppressions)

    env = {
        "ASAN_OPTIONS": ":".join(asan),
        "UBSAN_OPTIONS": ":".join(ubsan),
        "LSAN_OPTIONS": ":".join(lsan),
        # glib pools small allocations and zeroes nothing, which hides both
        # use-after-free and the origin of a leak from the sanitizer. These two
        # turn the pooling off; they are the documented way to run glib under a
        # memory checker.
        "G_SLICE": "always-malloc",
        "G_DEBUG": "gc-friendly",
    }

    return env


# ---------------------------------------------------------------------------
# Whose memory it is
# ---------------------------------------------------------------------------

def _origin(location):
    """"ours", "vendor", "library", or "" when the frame says nothing."""
    if not location:
        return ""

    if location.startswith("("):
        # No debug info for this frame: "(/usr/lib/libpango.so.0+0x2f1a3)".
        module = location[1:].split("+", 1)[0].rstrip(")")
        if os.path.basename(module) == "medit":
            return "ours"
        return "ours" if _under(module, SOURCE_ROOT) else "library"

    match = _LOCATION.match(location)
    if not match:
        return ""

    path = match.group(1)
    if not os.path.isabs(path):
        # Recorded relative to wherever the compiler ran, which for this tree is
        # the build directory inside it -- so a relative path is ours only if
        # the tree really holds that file. glibc's debug info says things like
        # "libio/vasprintf.c", which would otherwise read as one of ours.
        candidate = os.path.join(SOURCE_ROOT, path)
        if not os.path.exists(candidate):
            return "library"
        path = candidate

    if _under(path, VENDOR_ROOT):
        return "vendor"

    return "ours" if _under(path, SOURCE_ROOT) else "library"


def _under(path, root):
    path = os.path.normpath(path)
    return path == root or path.startswith(root + os.sep)


def _short(location):
    """The part of a frame worth printing."""
    if location.startswith("("):
        return os.path.basename(location[1:].split("+", 1)[0].rstrip(")"))

    match = _LOCATION.match(location)
    if not match:
        return location

    path = match.group(1)
    if os.path.isabs(path) and _under(path, SOURCE_ROOT):
        path = os.path.relpath(path, SOURCE_ROOT)

    return "%s:%s" % (path, match.group(2))


def _describe(frame):
    func, location = frame
    if not location:
        return func
    if not func:
        return _short(location)
    return "%s (%s)" % (func, _short(location))


def _is_wrapper(func):
    return func in _WRAPPER_NAMES or func.startswith(_WRAPPER_PREFIXES)


def _stack(lines, start, limit=48):
    """The frames of the record that begins at `start`."""
    frames = []

    for line in lines[start:start + limit]:
        match = _FRAME.match(line)
        if match:
            frames.append((match.group(2) or "", match.group(3) or ""))
        elif frames:
            break

    return frames


def _site(frames):
    """(where it was allocated, whose code that is)."""
    for frame in frames:
        if _is_wrapper(frame[0]):
            continue
        return _describe(frame), _origin(frame[1])

    if frames:
        return _describe(frames[0]), _origin(frames[0][1])

    return "", ""


def _ours(frames):
    """The nearest frame of our own, which is what LSan would have blamed."""
    for frame in frames:
        if _origin(frame[1]) in ("ours", "vendor") and not _is_wrapper(frame[0]):
            return _describe(frame)

    return ""


def gates(finding):
    """Whether this finding fails the test.

    Only leaks are ever filtered, and only those a library allocated for its own
    use. A leak whose stack could not be classified gates: an unreadable stack
    is a reason to look, not a reason to pass.
    """
    if finding["kind"] != "LeakSanitizer":
        return True

    return finding.get("origin") != "library"


# ---------------------------------------------------------------------------
# Reading the logs
# ---------------------------------------------------------------------------

def _parse_asan(path, findings):
    with open(path, errors="replace") as f:
        lines = f.read().splitlines()

    for i, line in enumerate(lines):
        error = _ERROR.search(line)
        if error and "detected memory leaks" not in error.group(2):
            frames = _stack(lines, i + 1)
            where, origin = _site(frames)
            findings.append({
                "kind": error.group(1),
                "what": error.group(2).strip(),
                "where": _describe(frames[0]) if frames else "",
                "origin": origin,
                "allocated": where,
            })
            continue

        leak = _LEAK.match(line)
        if leak:
            frames = _stack(lines, i + 1)
            where, origin = _site(frames)
            findings.append({
                "kind": "LeakSanitizer",
                "what": "%s leak of %s bytes in %s objects" % (
                    leak.group(1).lower(), leak.group(2), leak.group(3)),
                "bytes": int(leak.group(2)),
                "where": where,
                "origin": origin,
                # What the old report used to name, kept because it is the frame
                # a developer recognises and sometimes the one to look at.
                "blamed": _ours(frames),
            })


def _parse_ubsan(path, findings):
    with open(path, errors="replace") as f:
        lines = f.read().splitlines()

    for i, line in enumerate(lines):
        match = _RUNTIME.match(line)
        if match:
            findings.append({
                "kind": "UndefinedBehaviorSanitizer",
                "what": match.group(4).strip(),
                "where": "%s:%s" % (os.path.basename(match.group(1)), match.group(2)),
                "origin": _origin("%s:%s" % (match.group(1), match.group(2))),
            })


def analyse(log_dir):
    """Read every log the sanitizers left and return the findings."""
    findings = []

    for path in sorted(glob.glob(os.path.join(log_dir, ASAN_PREFIX + ".*"))):
        _parse_asan(path, findings)

    for path in sorted(glob.glob(os.path.join(log_dir, UBSAN_PREFIX + ".*"))):
        _parse_ubsan(path, findings)

    return findings


def summarise(findings):
    """Findings grouped by kind and site, so a repeated one is counted once."""
    counts = {}
    origins = {}
    sites = {}

    for finding in findings:
        counts[finding["kind"]] = counts.get(finding["kind"], 0) + 1
        origin = finding.get("origin") or "unknown"
        origins[origin] = origins.get(origin, 0) + 1
        key = (finding["kind"], finding["what"], finding["where"], origin)
        sites[key] = sites.get(key, 0) + 1

    unique = [
        {"kind": k, "what": w, "where": p, "origin": o,
         "gates": gates({"kind": k, "origin": o}), "count": n}
        for (k, w, p, o), n in sites.items()
    ]

    # What fails the test first, and the most repeated of those at the top.
    unique.sort(key=lambda item: (not item["gates"], -item["count"], item["where"]))

    gating = [f for f in findings if gates(f)]

    return {
        "total": len(findings),
        "gating": len(gating),
        "by_kind": counts,
        "by_origin": origins,
        "leaked_bytes": sum(f.get("bytes", 0) for f in findings),
        "gating_leaked_bytes": sum(f.get("bytes", 0) for f in gating),
        "unique": unique,
    }


def report(log_dir, enabled):
    """Write sanitizer.json next to the logs and return (summary, ok).

    The logs are read whether or not the build was configured with sanitizers.
    enabled only names what was asked for, so that a binary someone sanitized by
    hand still has its findings reported rather than silently dropped.
    """
    findings = analyse(log_dir)
    summary = summarise(findings)
    summary["enabled"] = bool(enabled)
    summary["sanitizers"] = enabled or ""

    with open(os.path.join(log_dir, "sanitizer.json"), "w") as f:
        json.dump(summary, f, indent=2, sort_keys=True)

    return summary, summary["gating"] == 0


def format_summary(summary):
    if not summary["enabled"]:
        return "sanitizers: not built in"

    filtered = summary["total"] - summary["gating"]
    tail = ""
    if filtered:
        tail = ", %d library leak%s filtered (%d bytes)" % (
            filtered, "" if filtered == 1 else "s",
            summary["leaked_bytes"] - summary["gating_leaked_bytes"])

    if summary["gating"] == 0:
        return "sanitizers: clean (%s)%s" % (summary["sanitizers"], tail)

    lines = ["sanitizers: %d findings (%s)%s" % (
        summary["gating"], summary["sanitizers"], tail)]

    if summary["gating_leaked_bytes"]:
        lines[0] += ", %d bytes leaked" % summary["gating_leaked_bytes"]

    shown = [item for item in summary["unique"] if item["gates"]]

    for item in shown[:20]:
        lines.append("  %-28s %s %s" % (
            item["kind"], item["what"], ("at " + item["where"]) if item["where"] else ""))

    if len(shown) > 20:
        lines.append("  ... and %d more distinct findings" % (len(shown) - 20))

    return "\n".join(lines)
