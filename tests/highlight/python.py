# A comment with a keyword in it: import
import sys


class Greeter(object):
    """A docstring, which is a string and not a comment."""

    def __init__(self, name='world'):
        self.name = name
        self.count = 0o17

    def greet(self):
        print("hello, %s" % self.name, file=sys.stderr)
        return f"{self.name!r} greeted {self.count} times"
