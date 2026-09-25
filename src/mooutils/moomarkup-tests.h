/*
 *   mooutils/moomarkup-tests.h
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#ifndef MOO_MARKUP_TESTS_H
#define MOO_MARKUP_TESTS_H


#ifdef MOO_ENABLE_UNIT_TESTS


G_BEGIN_DECLS

/* moo_markup_text_node_add_text()'s growth, measured by appending many
   chunks to one text node; nothing unless MOO_PERF is set. The function it
   measures is file-static, so the test lives in moomarkup.cpp itself rather
   than in a perf file of its own. */
void        _moo_add_moomarkup_perf_tests (void);

G_END_DECLS

#endif /* MOO_ENABLE_UNIT_TESTS */

#endif /* MOO_MARKUP_TESTS_H */
