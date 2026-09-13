/*
 *   mooutils/moo-unit-tests.h
 *
 *   Copyright (C) 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The unit tests, which live inside medit and are run by medit --unit-test.
 *
 * Not a second binary, and not a second build: the tests are compiled into the
 * program when ENABLE_UNIT_TESTS asks for it, which a UI test build already
 * does -- so the binary that runs them is the same sanitized binary the UI
 * tests drive, and the address sanitizer is watching what they touch. An
 * ordinary build, and every package build, has none of this compiled at all.
 *
 * What belongs here is what a UI test can only reach through the whole
 * program: the arithmetic of the language server protocol, the shapes a reply
 * can take, a parser. What does not belong here is anything about widgets --
 * drawing, events and the GTK+2/GTK+3 split are what tests/ is for, and a unit
 * test that mocks a toolkit tests the mock.
 *
 * Nothing in here needs a display. GtkTextBuffer and GtkTreeStore are objects
 * rather than widgets and work with no gtk_init() and no DISPLAY at all, which
 * is measured rather than assumed and is why the text side of the client can
 * be tested this way.
 *
 * The framework is glib's, so there is no new dependency: g_test_add_func(),
 * g_assert_cmpint() and the rest come with the glib medit already links.
 */

#ifndef MOO_UNIT_TESTS_H
#define MOO_UNIT_TESTS_H


#ifdef MOO_ENABLE_UNIT_TESTS


G_BEGIN_DECLS

/*
 * Runs the tests and returns what the program should exit with. paths, when it
 * is not NULL, is a NULL-terminated list of test paths ("/lsp/position"), each
 * of which may name a whole subtree; without it everything registered runs.
 */
int         moo_unit_tests_run      (char      **paths);

/* Prints the path of every registered test, one per line. */
void        moo_unit_tests_list     (void);

G_END_DECLS

#endif /* MOO_ENABLE_UNIT_TESTS */

#endif /* MOO_UNIT_TESTS_H */
