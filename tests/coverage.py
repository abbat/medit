"""Merges lcov reports, reports what they say, and compares it with the floor.

The two toolkits are two builds, so they are two profiles and two exported
reports; this puts them together. Line for line, because that is what they
have in common -- the same sources compiled twice, once with GTK+2 and once
with GTK+3, so a line the GTK+3 build never compiled is still a line the
GTK+2 build may have run.

    python3 tests/coverage.py build*/coverage/medit.info \\
            --output coverage.info --floor tests/coverage.floor

The exit code is the verdict: 1 when line coverage is below the floor, 0
otherwise, and 0 with a note when there is no floor file to compare with.

Merging the profiles instead -- llvm-profdata takes as many as it is given --
would mean carrying two instrumented binaries between jobs, a few hundred
megabytes each, to produce a number that this arrives at from two files of a
megabyte. llvm-cov has already done the part that needs the binary.
"""

import argparse
import collections
import os
import sys


class File(object):
    """One source file's counters, as lcov records them.

    lines maps a line number to how many times it ran, funcs a function name to
    how many times it was called. Both are summed when reports are merged: a
    line run by the GTK+2 build and by the GTK+3 build ran twice, and a line
    only one of them compiled keeps the count of the build that had it.
    """

    def __init__(self):
        self.lines = collections.Counter()
        self.funcs = collections.Counter()
        self.func_lines = {}

    def merge(self, other):
        self.lines.update(other.lines)
        self.funcs.update(other.funcs)
        self.func_lines.update(other.func_lines)


def read(path, root):
    """Reads one lcov file into {source path: File}.

    Paths are made relative to root when they are under it. llvm-cov writes
    absolute ones, and the absolute path of a checkout is not the same thing in
    two jobs -- or in a container and on the machine that reads its artifact.
    """
    files = {}
    current = None

    with open(path) as handle:
        for line in handle:
            line = line.strip()

            if line.startswith("SF:"):
                name = os.path.relpath(line[3:], root)
                current = files.setdefault(name, File())
            elif current is None:
                continue
            elif line.startswith("DA:"):
                number, _, count = line[3:].partition(",")
                current.lines[int(number)] += int(count)
            elif line.startswith("FN:"):
                # llvm writes "FN:<line>,<name>". Other producers of lcov write
                # "FN:<start>,<end>,<name>", and a name is worth reading either
                # way -- it costs one test and saves a report that is silently
                # about nothing.
                number, _, name = line[3:].partition(",")
                head, _, rest = name.partition(",")
                if rest and head.isdigit():
                    name = rest
                current.func_lines[name] = int(number)
                current.funcs.setdefault(name, 0)
            elif line.startswith("FNDA:"):
                count, _, name = line[5:].partition(",")
                current.funcs[name] += int(count)
            elif line == "end_of_record":
                current = None

    return files


def merge(reports):
    total = {}

    for files in reports:
        for name, one in files.items():
            total.setdefault(name, File()).merge(one)

    return total


def write(files, path):
    """Writes the merged reports back out as lcov, for anything that reads it.

    Branch data is not carried across: llvm-cov exports it, nothing here reads
    it, and a BRDA line whose counts were not merged would be a lie in a file
    that otherwise is not one.
    """
    with open(path, "w") as handle:
        for name in sorted(files):
            one = files[name]
            handle.write("SF:%s\n" % name)

            for func, line in sorted(one.func_lines.items(), key=lambda item: item[1]):
                handle.write("FN:%d,%s\n" % (line, func))

            for func, count in sorted(one.funcs.items()):
                handle.write("FNDA:%d,%s\n" % (count, func))

            handle.write("FNF:%d\n" % len(one.funcs))
            handle.write("FNH:%d\n" % sum(1 for count in one.funcs.values() if count))

            for number in sorted(one.lines):
                handle.write("DA:%d,%d\n" % (number, one.lines[number]))

            handle.write("LF:%d\n" % len(one.lines))
            handle.write("LH:%d\n" % sum(1 for count in one.lines.values() if count))
            handle.write("end_of_record\n")


def count(files):
    """(lines hit, lines found, functions hit, functions found)."""
    hit = found = fhit = ffound = 0

    for one in files.values():
        found += len(one.lines)
        hit += sum(1 for c in one.lines.values() if c)
        ffound += len(one.funcs)
        fhit += sum(1 for c in one.funcs.values() if c)

    return hit, found, fhit, ffound


def percent(hit, found):
    return 100.0 * hit / found if found else 0.0


def by_directory(files):
    """Groups by the two leading path components, which is one subsystem each.

    src/mooedit, src/mooutils, src/plugins/lsp: deeper than that is one row per
    file and shallower is one row for everything.
    """
    groups = collections.defaultdict(dict)

    for name, one in files.items():
        parts = os.path.dirname(name).split(os.sep)
        groups[os.sep.join(parts[:3]) or name][name] = one

    return groups


def read_floor(path):
    """The percentage the tests must not fall below, or None if there is none.

    The file is a line with a number in it and as many comment lines as it
    takes to say why the number is what it is.
    """
    if not path or not os.path.exists(path):
        return None

    with open(path) as handle:
        for line in handle:
            line = line.strip()
            if line and not line.startswith("#"):
                return float(line)

    return None


def cell(hit, found):
    return "%.2f%% (%d / %d)" % (percent(hit, found), hit, found)


def markdown(named, merged, floor):
    out = ["## Coverage", "", "| report | lines | functions |", "|---|---|---|"]

    for name, files in named:
        hit, found, fhit, ffound = count(files)
        out.append("| %s | %s | %s |" % (name, cell(hit, found), cell(fhit, ffound)))

    hit, found, fhit, ffound = count(merged)
    out.append("| **merged** | **%s** | **%s** |"
               % (cell(hit, found), cell(fhit, ffound)))

    total = percent(hit, found)
    out.append("")

    if floor is None:
        out.append("No floor to compare with. Write `%.1f` into "
                   "`tests/coverage.floor` to make this one." % (total - 0.3))
    elif total < floor:
        out.append("**Below the floor of %.2f%% by %.2f pp.** If the drop is meant --"
                   " covered code was deleted, a test was retired -- lower"
                   " `tests/coverage.floor` in the same commit, with the reason."
                   % (floor, floor - total))
    else:
        out.append("Floor is %.2f%%, so there is %.2f pp of room. Raise it to `%.1f`"
                   " when this number is where it stays."
                   % (floor, total - floor, total - 0.3))

    out += ["", "<details><summary>By directory</summary>", "",
            "| directory | lines | functions |", "|---|---|---|"]

    groups = by_directory(merged)

    for name in sorted(groups):
        hit, found, fhit, ffound = count(groups[name])
        out.append("| %s | %s | %s |" % (name, cell(hit, found), cell(fhit, ffound)))

    out += ["", "</details>", ""]

    return "\n".join(out)


def main(argv):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("reports", nargs="+", help="the lcov files to merge")
    p.add_argument("--root", default=os.getcwd(),
                   help="what the paths in them are relative to")
    p.add_argument("--floor", help="file holding the percentage not to fall below")
    p.add_argument("--output", help="where to write the merged lcov")
    p.add_argument("--summary", help="where to append the markdown report")
    args = p.parse_args(argv)

    named = []

    for path in args.reports:
        name = os.path.basename(path)
        for cut in ("coverage-", "medit-"):
            if name.startswith(cut):
                name = name[len(cut):]
        named.append((os.path.splitext(name)[0], read(path, args.root)))

    merged = merge(files for _, files in named)

    if not merged:
        print("FAIL: %s named no source files" % ", ".join(args.reports))
        return 1

    if args.output:
        write(merged, args.output)

    floor = read_floor(args.floor)
    report = markdown(named, merged, floor)

    if args.summary:
        with open(args.summary, "a") as handle:
            handle.write(report + "\n")

    print(report)

    hit, found = count(merged)[:2]
    total = percent(hit, found)

    if floor is not None and total < floor:
        print("FAIL: line coverage %.2f%% is below the floor of %.2f%%"
              % (total, floor))
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
