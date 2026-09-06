"""The per-test sandbox: a private temp root, a private X server, a private
session bus.

Every test gets its own copy of all three, which is what makes running them in
parallel safe: two tests never share a settings file, a display or a bus name.
"""

import os
import shutil
import signal
import subprocess
import tempfile
import time


# The at-spi bus socket is created under XDG_RUNTIME_DIR, and a UNIX socket path
# is limited to about 108 bytes. A root under a build directory or a scratch
# directory is long enough to go over it, and the only symptom is one line on
# medit's stderr --
#
#   atk-bridge: Couldn't listen on dbus server: Socket name too long
#
# -- after which the accessibility tree never appears and the test times out
# looking for a window that is on screen. So the root goes directly under /tmp
# with a short name.
TMP_ROOT = "/tmp"
TMP_PREFIX = "mui."


def make_root(tmp_root=TMP_ROOT):
    """Create the sandbox root and the XDG directories inside it."""
    root = tempfile.mkdtemp(prefix=TMP_PREFIX, dir=tmp_root)

    dirs = {
        "XDG_DATA_HOME": os.path.join(root, "data"),
        "XDG_CONFIG_HOME": os.path.join(root, "config"),
        "XDG_CACHE_HOME": os.path.join(root, "cache"),
        "XDG_STATE_HOME": os.path.join(root, "state"),
        "XDG_RUNTIME_DIR": os.path.join(root, "run"),
        "TMPDIR": os.path.join(root, "tmp"),
    }

    for path in dirs.values():
        os.makedirs(path, exist_ok=True)

    # dbus refuses a runtime directory anyone else can read
    os.chmod(dirs["XDG_RUNTIME_DIR"], 0o700)

    return root, dirs


def remove_root(root):
    if root and os.path.isdir(root) and os.path.basename(root).startswith(TMP_PREFIX):
        shutil.rmtree(root, ignore_errors=True)


def start_x(root, log_path, screen="1400x900x24", timeout=20):
    """Start an Xvfb and return (process, display).

    The display number comes from Xvfb through -displayfd rather than being
    picked here: two tests starting at the same moment would otherwise both find
    :99 free and both try to take it.
    """
    handshake = os.path.join(root, "displayfd")
    fd = os.open(handshake, os.O_RDWR | os.O_CREAT, 0o600)
    log = open(log_path, "wb")

    try:
        proc = subprocess.Popen(
            ["Xvfb", "-displayfd", str(fd), "-screen", "0", screen, "-nolisten", "tcp"],
            stdout=log, stderr=subprocess.STDOUT, pass_fds=(fd,))
    finally:
        os.close(fd)
        log.close()

    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError("Xvfb exited with %d, see %s" % (proc.returncode, log_path))
        with open(handshake) as f:
            text = f.read().strip()
        if text.isdigit():
            return proc, ":" + text
        time.sleep(0.1)

    stop(proc)
    raise RuntimeError("Xvfb did not report a display number within %ds" % timeout)


def stop(proc, timeout=5):
    """Stop a process by PID, politely first.

    By PID and never by name: pkill on a name has already killed a developer's
    own window manager once, and matching a command line matches the shell that
    started it.
    """
    if proc is None or proc.poll() is not None:
        return

    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=timeout)
