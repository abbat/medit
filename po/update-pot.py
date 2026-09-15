#!/usr/bin/env python3
"""Rebuild po/medit.pot from the files POTFILES.in lists.

    python3 po/update-pot.py
    msgmerge --update --backup=none po/ru.po po/medit.pot

intltool used to do this and is gone.  Two of the things it did are done here
before xgettext runs: the [type: gettext/glade] prefixes are dropped (xgettext
reads .ui files as GtkBuilder by itself), and the files that mark strings with
an underscore on an attribute or a key -- the ui .xml files and the .desktop
-- are rewritten as a C header of N_() calls, the way intltool named them.
Everything else xgettext handles directly.
"""

import os
import re
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATTR = re.compile(r'_([a-z]+)="([^"]*)"')
KEY = re.compile(r'^_([A-Za-z]+)=(.*)$')
COMMENT = re.compile(r'<!--(.*?)-->', re.S)


def marked(path):
    """The translatable strings of an intltool-style file, with their comments."""
    out, comment = [], None
    for line in open(os.path.join(TOP, path), encoding='utf-8'):
        seen = COMMENT.search(line)
        if seen:
            comment = seen.group(1).strip()
        key = KEY.match(line)
        strings = [key.group(2)] if key else [m.group(2) for m in ATTR.finditer(line)]
        for string in strings:
            out.append((comment, string))
            comment = None
    return out


def as_header(path):
    """Write the strings of one file as the C header intltool would have."""
    lines = []
    for comment, string in marked(path):
        if comment:
            lines.append('/* %s */\n' % comment)
        lines.append('char *s = N_("%s");\n' % string.replace('"', '\\"'))
    header = os.path.join(TOP, path + '.h')
    open(header, 'w', encoding='utf-8').writelines(lines)
    return path + '.h'


def main():
    files, generated = [], []
    for line in open(os.path.join(TOP, 'po/POTFILES.in'), encoding='utf-8'):
        line = re.sub(r'^\[type: gettext/glade\]\s*', '', line.strip())
        if not line or line.startswith('#'):
            continue
        if line.endswith('.xml') or line.endswith('.desktop.in'):
            line = as_header(line)
            generated.append(line)
        files.append(line)

    listing = os.path.join(TOP, 'po/.potfiles')
    open(listing, 'w', encoding='utf-8').write('\n'.join(files) + '\n')
    try:
        subprocess.check_call([
            'xgettext', '--directory=.', '--files-from=po/.potfiles',
            '--from-code=UTF-8', '--keyword=_', '--keyword=N_', '--keyword=Q_',
            '--keyword=C_:1c,2', '--keyword=NC_:1c,2', '--add-comments',
            '--package-name=medit',
            '--msgid-bugs-address=https://github.com/abbat/medit/issues',
            '--output=po/medit.pot'], cwd=TOP)
    finally:
        for path in generated + ['po/.potfiles']:
            os.unlink(os.path.join(TOP, path))


if __name__ == '__main__':
    sys.exit(main())
