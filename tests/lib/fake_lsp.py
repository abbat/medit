"""A language server that does what the test told it to, and writes down what it was asked.

Not a real one. A test about the client cannot be a test of clangd as well:
what a real server answers depends on its version, on the index it built and on
the machine it runs on, and none of the three is a thing a test can hold still.
This one answers out of a file the test wrote before medit started -- which
capabilities it claims, what it says a symbol is defined at, what it offers for
completion -- so the assertion is about what medit does with the answer.

It is also the only oracle for the half of the protocol that has no pixels: the
notifications medit sends. Every message that arrives is appended to a log as
one line of JSON, so a test can say "medit announced the document, once, with
this text" rather than guessing from what appeared on screen.

Started as

    python3 fake_lsp.py <scenario.json>

and the scenario is re-read at every start, so a server that is restarted can
be told to behave differently the second time.

The scenario, all of it optional:

    log                 where to append the messages that arrive
    starts              a file to append a byte to at every start, for counting
    mode                normal (default), exit-at-once, exit-after-initialize,
                        or no-answer -- initialize is never answered
    exit_after          seconds to live after initialize, then exit(1)
    capabilities        merged over the defaults below; a null value drops one
    diagnostics         published for a document after it is opened
    diagnostics_delay   seconds to wait before publishing them
    replies             method -> the result to answer requests with
    errors              method -> {code, message} to answer with instead
    reply_delay         method -> seconds to think before answering
"""

import json
import os
import sys
import threading
import time


# What the server claims it can do unless the scenario says otherwise. Every
# provider is on: a test that wants one missing turns it off by name, which
# reads as the point of that test.
DEFAULT_CAPABILITIES = {
    "textDocumentSync": {"openClose": True, "change": 1, "save": True},
    "definitionProvider": True,
    "typeDefinitionProvider": True,
    "implementationProvider": True,
    "documentSymbolProvider": True,
    "hoverProvider": True,
    "completionProvider": {"triggerCharacters": ["."]},
}


def read_message(stream):
    """One framed message, or None at end of input.

    The framing is the protocol's: headers, a blank line, then exactly
    Content-Length bytes of JSON.
    """
    length = None

    while True:
        line = stream.readline()

        if not line:
            return None

        line = line.strip()

        if not line:
            break

        name, _, value = line.partition(b":")

        if name.strip().lower() == b"content-length":
            length = int(value.strip())

    if length is None:
        return None

    body = b""

    while len(body) < length:
        chunk = stream.read(length - len(body))
        if not chunk:
            return None
        body += chunk

    return json.loads(body.decode("utf-8"))


def write_message(stream, payload):
    body = json.dumps(payload).encode("utf-8")
    stream.write(b"Content-Length: %d\r\n\r\n" % len(body))
    stream.write(body)
    stream.flush()


class FakeServer(object):
    def __init__(self, scenario_path):
        self.scenario_path = scenario_path
        self.scenario = {}
        self.started = time.time()
        self.out = sys.stdout.buffer
        self.stdin = sys.stdin.buffer

    # -- the scenario ------------------------------------------------------

    def load(self):
        try:
            with open(self.scenario_path) as f:
                self.scenario = json.load(f)
        except (FileNotFoundError, ValueError):
            self.scenario = {}

    def get(self, name, fallback=None):
        return self.scenario.get(name, fallback)

    def capabilities(self):
        capabilities = dict(DEFAULT_CAPABILITIES)

        for name, value in (self.get("capabilities") or {}).items():
            if value is None:
                capabilities.pop(name, None)
            else:
                capabilities[name] = value

        return capabilities

    # -- the log -----------------------------------------------------------

    def note(self, message):
        """Append one arriving message to the log.

        Opened and closed around every line: a server that is killed in the
        middle of a test has still written everything it saw, and a restarted
        one appends to the same file rather than replacing it.
        """
        path = self.get("log")

        if not path:
            return

        record = {
            "at": round(time.time() - self.started, 3),
            "pid": os.getpid(),
            "method": message.get("method"),
            "id": message.get("id"),
            "params": message.get("params"),
        }

        with open(path, "a") as f:
            f.write(json.dumps(record) + "\n")

    def count_start(self):
        path = self.get("starts")

        if path:
            with open(path, "a") as f:
                f.write("x")

    # -- answering ---------------------------------------------------------

    def reply(self, message_id, result):
        write_message(self.out, {"jsonrpc": "2.0", "id": message_id, "result": result})

    def reply_error(self, message_id, error):
        write_message(self.out, {"jsonrpc": "2.0", "id": message_id, "error": error})

    def notify(self, method, params):
        write_message(self.out, {"jsonrpc": "2.0", "method": method, "params": params})

    def publish_diagnostics(self, uri):
        diagnostics = self.get("diagnostics")

        if diagnostics is None:
            return

        delay = self.get("diagnostics_delay", 0)

        if delay:
            time.sleep(delay)

        self.notify("textDocument/publishDiagnostics",
                    {"uri": uri, "diagnostics": diagnostics})

    def answer_request(self, message):
        method = message.get("method")
        message_id = message.get("id")

        delay = (self.get("reply_delay") or {}).get(method)

        if delay:
            time.sleep(delay)

        error = (self.get("errors") or {}).get(method)

        if error is not None:
            self.reply_error(message_id, error)
            return

        replies = self.get("replies") or {}

        # A method the scenario says nothing about is answered with null,
        # which is what a server that found nothing answers.
        self.reply(message_id, replies.get(method))

    # -- the loop ----------------------------------------------------------

    def die_later(self):
        """Exit while the client thinks the server is working, for the restart test."""
        seconds = self.get("exit_after")

        if not seconds:
            return

        def wait_and_die():
            time.sleep(seconds)
            os._exit(1)

        thread = threading.Thread(target=wait_and_die, daemon=True)
        thread.start()

    def run(self):
        self.load()
        self.count_start()

        if self.get("mode") == "exit-at-once":
            return 1

        while True:
            # End of input means medit is gone: there is nobody left to
            # answer, and a server that stayed would outlive the sandbox it
            # was started in.
            message = read_message(self.stdin)

            if message is None:
                return 0

            self.note(message)

            method = message.get("method")

            if method == "initialize":
                if self.get("mode") == "no-answer":
                    continue

                self.reply(message.get("id"),
                           {"capabilities": self.capabilities(),
                            "serverInfo": {"name": "fake-lsp", "version": "1"}})

                if self.get("mode") == "exit-after-initialize":
                    return 1

                self.die_later()

            elif method == "textDocument/didOpen":
                document = (message.get("params") or {}).get("textDocument") or {}
                self.publish_diagnostics(document.get("uri"))

            elif method == "shutdown":
                self.reply(message.get("id"), None)

            elif method == "exit":
                return 0

            elif message.get("id") is not None:
                self.answer_request(message)


def main(argv):
    if len(argv) != 1:
        sys.stderr.write("usage: fake_lsp.py <scenario.json>\n")
        return 2

    return FakeServer(argv[0]).run()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
