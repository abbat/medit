"""A browser that does not open.

medit hands http and https URLs to gtk_show_uri(), which asks GIO for the
default application of the x-scheme-handler/http mime type. Inside the sandbox
that application is a two-line shell script which appends the URL to a file, so
"the license opened in a browser" becomes a string comparison instead of a
screenshot of whatever browser the machine happens to have.
"""

import os
import subprocess


DESKTOP_ID = "urlcatch.desktop"

_HANDLER = """#!/bin/sh
printf '%s\\n' "$1" >>"{log}"
"""

_DESKTOP = """[Desktop Entry]
Type=Application
Name=urlcatch
Exec={handler} %u
MimeType=x-scheme-handler/http;x-scheme-handler/https;
NoDisplay=true
Terminal=false
"""

_MIMEAPPS = """[Default Applications]
x-scheme-handler/http={desktop}
x-scheme-handler/https={desktop}
"""


def install(root, data_home, config_home):
    """Install the fake handler and return the path of the URL log."""
    log = os.path.join(root, "urls.txt")
    open(log, "w").close()

    handler = os.path.join(root, "urlcatch")
    with open(handler, "w") as f:
        f.write(_HANDLER.format(log=log))
    os.chmod(handler, 0o755)

    applications = os.path.join(data_home, "applications")
    os.makedirs(applications, exist_ok=True)

    with open(os.path.join(applications, DESKTOP_ID), "w") as f:
        f.write(_DESKTOP.format(handler=handler))

    os.makedirs(config_home, exist_ok=True)
    with open(os.path.join(config_home, "mimeapps.list"), "w") as f:
        f.write(_MIMEAPPS.format(desktop=DESKTOP_ID))

    # Builds the mimeinfo.cache GIO reads. Without it the desktop entry is still
    # found, but only after GIO has scanned every directory, which is slower and
    # depends on what else is installed on the machine.
    try:
        subprocess.run(["update-desktop-database", applications],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
    except FileNotFoundError:
        pass

    return log


def urls(log):
    """Every URL handed to a browser so far, in order."""
    try:
        with open(log) as f:
            return [line.strip() for line in f if line.strip()]
    except FileNotFoundError:
        return []
