/*
 *   mooutils/moo-unit-tests.cpp
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

#include "mooutils/moo-unit-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

/*
 * Every suite in one place, so that what is compiled in is a list somebody can
 * read rather than a set of constructors running in whatever order the linker
 * chose. Empty until a module has tests of its own to add.
 */
static void
add_all_tests (void)
{
}


/*
 * g_test_init() wants the program's own argv and takes the options it knows
 * out of it, so it is handed one of ours: the name, -l to list, and -p for each
 * path the caller named. What medit itself was started with has been parsed
 * already and is none of its business.
 *
 * The array is freed and the strings in it are not: g_test_init() keeps
 * argv[0] and shifts the rest about, so the block is ours to release and what
 * it pointed at is not. Both halves matter -- this runs inside a sanitized
 * binary, and a harness that leaks is a harness that reports leaks.
 */
static int
run (char    **paths,
     gboolean list_only)
{
    GPtrArray *args = g_ptr_array_new ();
    char **argv;
    char **allocated;
    int argc;
    int result;
    guint i;

    g_ptr_array_add (args, g_strdup ("medit --unit-test"));

    if (list_only)
        g_ptr_array_add (args, g_strdup ("-l"));

    for (i = 0; paths && paths[i]; ++i)
    {
        g_ptr_array_add (args, g_strdup ("-p"));
        g_ptr_array_add (args, g_strdup (paths[i]));
    }

    argc = (int) args->len;
    g_ptr_array_add (args, NULL);
    argv = (char**) g_ptr_array_free (args, FALSE);
    allocated = argv;

    /* nullptr and not NULL: the sentinel has to be a pointer, and in C++ a
       bare NULL is an int -- which -Wformat catches only in a strict build. */
    g_test_init (&argc, &argv, nullptr);

    add_all_tests ();

    result = g_test_run ();

    g_free (allocated);

    return result;
}


int
moo_unit_tests_run (char **paths)
{
    return run (paths, FALSE);
}


void
moo_unit_tests_list (void)
{
    run (NULL, TRUE);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
