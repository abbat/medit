"""Ctags lists symbols and jumps to the selected function.

# requires: MOO_BUILD_CTAGS

The document is opened from disk so the ctags plugin can parse its filename.
The UI test checks the complete path from the document plugin through the
ctags process and tree model to the window plugin's cursor movement.
"""

CONTENT = """#define ANSWER 42

struct Widget {
    int value;
};

static int helper(void)
{
    return ANSWER;
}

int main(void)
{
    return helper();
}
"""


def setup(s):
    s.plugin("Ctags")
    s.open(s.write("workdir/example.c", CONTENT))


def run(t):
    t.menu("View", "Panes", "Functions")
    tree = t.wait(lambda: ctags_tree(t), "the Functions pane")

    t.wait(lambda: expected_symbols(rows(t, tree)),
           "ctags symbols to appear; the tree holds %s" % rows(t, tree))
    t.log("ok: ctags listed the functions, macro and structure")

    main = t.need(tree, role="table cell", name="main",
                  what="the main function")
    t.click(main)

    t.wait(lambda: cursor(t) == "Line: 12 Col: 1",
           "the cursor to move to main; it is at %s" % cursor(t))
    t.log("ok: activating a ctags row moves the cursor")


def ctags_tree(t):
    trees = t.on_screen(t.find_all(t.frame, role="tree table", depth=30))
    return trees[0] if len(trees) == 1 else None


def rows(t, tree):
    return [cell.name for cell in t.find_all(tree, role="table cell", depth=5)
            if cell.name]


def expected_symbols(names):
    required = ("Functions", "helper", "main", "Macros", "ANSWER")

    if not all(name in names for name in required):
        return False

    return any(name == "Widget" or name.endswith(" Widget") for name in names)


def cursor(t):
    label = t.find(t.frame, role="label", name_prefix="Line:", depth=25)
    return label.name if label is not None else None
