"""Sanitizer environment and log analysis.

A UI test has two verdicts, not one. The first is whether the scenario did what
it was supposed to do; the second is what the sanitizers saw while it did it,
which is the more interesting half -- a leak or a use-after-free in a dialog is
invisible to a test that only checks that the dialog appeared.

Both have to pass for the test to pass.
"""

import glob
import json
import os
import re


ASAN_PREFIX = "asan"
UBSAN_PREFIX = "ubsan"

# "==1234==ERROR: AddressSanitizer: heap-use-after-free on address 0x..."
_ERROR = re.compile(r"==\d+==ERROR: (AddressSanitizer|LeakSanitizer): (.+)")
# "Direct leak of 40 byte(s) in 1 object(s) allocated from:"
_LEAK = re.compile(r"^(Direct|Indirect) leak of (\d+) byte\(s\) in (\d+) object\(s\)")
# "    #0 0x5555 in moo_foo src/mooutils/moofoo.c:42:9"
_FRAME = re.compile(r"^\s*#(\d+) 0x[0-9a-f]+ in (\S+)(?: ([^\s]+:\d+)(?::\d+)?)?")
# "src/mooedit/mooedit.cpp:12:5: runtime error: signed integer overflow: ..."
_RUNTIME = re.compile(r"^(.*?):(\d+):(?:(\d+):)? runtime error: (.+)$")


def options(log_dir, suppressions=None, leak_check=False):
    """The environment the sanitizer runtimes read.

    log_path makes each runtime write to a file of its own instead of stderr,
    where it would be mixed in with gtk warnings and with medit's own output.
    Each runtime appends its pid to the name.

    Leak checking is off unless asked for. It was measured before this harness
    existed: a clean exit reports 633 records and 121 KB, of which 344 are
    purely library and the rest are attributed to our code only because our
    frame is the nearest one below pango's font and shaping caches -- the
    largest group, 101 records, points at update_tab_width(), which frees all
    three of the things it allocates. Suppressing those by the function names
    they are blamed on would suppress the next real leak in the same function,
    so the file is a deliberate opt-in for someone chasing a specific leak
    rather than a gate:

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


def _first_frame(lines, start):
    for line in lines[start:start + 12]:
        match = _FRAME.match(line)
        if match and match.group(3):
            return "%s (%s)" % (match.group(2), match.group(3))
        if match:
            return match.group(2)
    return ""


def _parse_asan(path, findings):
    with open(path, errors="replace") as f:
        lines = f.read().splitlines()

    for i, line in enumerate(lines):
        error = _ERROR.search(line)
        if error and "detected memory leaks" not in error.group(2):
            findings.append({
                "kind": error.group(1),
                "what": error.group(2).strip(),
                "where": _first_frame(lines, i + 1),
            })
            continue

        leak = _LEAK.match(line)
        if leak:
            findings.append({
                "kind": "LeakSanitizer",
                "what": "%s leak of %s bytes in %s objects" % (
                    leak.group(1).lower(), leak.group(2), leak.group(3)),
                "bytes": int(leak.group(2)),
                "where": _first_frame(lines, i + 1),
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
    sites = {}

    for finding in findings:
        counts[finding["kind"]] = counts.get(finding["kind"], 0) + 1
        key = (finding["kind"], finding["what"], finding["where"])
        sites[key] = sites.get(key, 0) + 1

    unique = [
        {"kind": k, "what": w, "where": p, "count": n}
        for (k, w, p), n in sorted(sites.items(), key=lambda kv: -kv[1])
    ]

    leaked = sum(f.get("bytes", 0) for f in findings if f["kind"] == "LeakSanitizer")

    return {"total": len(findings), "by_kind": counts, "leaked_bytes": leaked, "unique": unique}


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

    return summary, summary["total"] == 0


def format_summary(summary):
    if not summary["enabled"]:
        return "sanitizers: not built in"

    if summary["total"] == 0:
        return "sanitizers: clean (%s)" % summary["sanitizers"]

    lines = ["sanitizers: %d findings (%s)" % (summary["total"], summary["sanitizers"])]

    if summary["leaked_bytes"]:
        lines[0] += ", %d bytes leaked" % summary["leaked_bytes"]

    for item in summary["unique"][:20]:
        lines.append("  %-28s %s %s" % (
            item["kind"], item["what"], ("at " + item["where"]) if item["where"] else ""))

    if len(summary["unique"]) > 20:
        lines.append("  ... and %d more distinct findings" % (len(summary["unique"]) - 20))

    return "\n".join(lines)
