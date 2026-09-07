"""The per-test sandbox: a private temp root, a private X server, a private
session bus.

Every test gets its own copy of all three, which is what makes running them in
parallel safe: two tests never share a settings file, a display or a bus name.
"""

import contextlib
import fcntl
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
    """Create the sandbox root and the per-test directories inside it.

    The keys are the names of the environment variables that point at them, so
    the caller hands the whole dict to the environment of the test.
    """
    root = tempfile.mkdtemp(prefix=TMP_PREFIX, dir=tmp_root)

    dirs = {
        # A home of its own, not only XDG directories. medit itself is happy
        # with the XDG ones, but the terminal pane starts a login-less shell
        # that reads the rc files of whoever runs the tests: with the real HOME
        # the shell came up with the developer's prompt, wrote to the
        # developer's history file, and behaved differently in CI, where that
        # home does not exist.
        "HOME": os.path.join(root, "home"),
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


# X puts its sockets in a directory it insists on creating itself, and several
# servers starting at the same second race to create it: one wins, the others
# print "_XSERVTransmkdir: ERROR: Cannot create /tmp/.X11-unix" and then treat
# every display as taken. Creating it first, once, costs nothing and is the
# difference between thirteen tests starting and twelve.
X_SOCKET_DIR = "/tmp/.X11-unix"


def ensure_x_socket_dir():
    try:
        # 1777 is what this directory is required to be -- every user's X
        # server puts a socket in it, and the sticky bit is what keeps them
        # from removing each other's. It is also what the system's own copy
        # already is.
        os.makedirs(X_SOCKET_DIR, mode=0o1777, exist_ok=True)
        os.chmod(X_SOCKET_DIR, 0o1777)
    except OSError:
        # Not ours to fix -- somebody else's, with the right permissions
        # already, or a system where this is not where the sockets go.
        pass


def display_answers(display, timeout=5, interval=0.2):
    """Whether anything is actually listening on that display.

    Asked because a display number is not a promise: under a race Xvfb has
    reported a number and then not served it, and the only symptom, a minute
    later, is medit's "cannot open display" in a log nobody reads.
    """
    env = dict(os.environ, DISPLAY=display)
    deadline = time.time() + timeout

    while time.time() < deadline:
        done = subprocess.run(["xdotool", "getdisplaygeometry"],
                              env=env, capture_output=True)
        if done.returncode == 0:
            return True
        time.sleep(interval)

    return False


# Only one X server may be choosing a display number at a time. Xvfb picks a
# number, binds, and only then reports it, but a server that cannot create the
# socket file for a number still starts if it got the abstract socket -- it
# prints "server already running" and carries on -- so two servers starting
# together can end up sharing a number, one of them serving the socket file and
# the other the abstract socket. Everything then works until the first of the
# two exits and takes the socket file with it, and the symptom is a test whose
# display answers xdotool and refuses medit half a second later.
XVFB_LOCK = "mui.xvfb.lock"


@contextlib.contextmanager
def display_lock(tmp_root):
    fd = os.open(os.path.join(tmp_root, XVFB_LOCK), os.O_RDWR | os.O_CREAT, 0o600)

    try:
        fcntl.flock(fd, fcntl.LOCK_EX)
        yield
    finally:
        os.close(fd)


def start_x(root, log_path, screen="1400x900x24", timeout=20, attempts=3):
    """Start an Xvfb, check that it serves what it reported, and return it."""
    ensure_x_socket_dir()

    for attempt in range(attempts):
        with display_lock(os.path.dirname(root)):
            proc, display = _start_x_once(root, log_path, screen, timeout)

        if display_answers(display):
            return proc, display

        print("    the X server reported %s and does not answer on it, starting "
              "another (attempt %d of %d)" % (display, attempt + 1, attempts))
        stop(proc)

    raise RuntimeError("no X server that answers on the display it reported, "
                       "see %s" % log_path)


def _start_x_once(root, log_path, screen, timeout):
    """Start an Xvfb and return (process, display).

    The display number comes from Xvfb through -displayfd rather than being
    picked here: two tests starting at the same moment would otherwise both find
    :99 free and both try to take it.
    """
    # Xvfb writes the display number it settled on, with a newline after it,
    # which is the only thing that says the number is complete: a two digit
    # number arrives in two writes often enough to matter, and reading "1" out
    # of "12" hands the test a display belonging to another test, or to nobody
    # at all -- the symptom is a test that fails a minute later with "cannot
    # open display" while every other test passes.
    handshake = os.path.join(root, "displayfd")
    fd = os.open(handshake, os.O_RDWR | os.O_CREAT | os.O_TRUNC, 0o600)

    # Appended to, not truncated: a second attempt would otherwise throw away
    # what the first one said about why there had to be a second attempt.
    log = open(log_path, "ab")

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
            text = f.read()

        if text.endswith("\n") and text.strip().isdigit():
            return proc, ":" + text.strip()
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
