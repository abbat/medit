/*
 *   plugins/mooquickopen-tests.h
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#ifndef MOO_QUICK_OPEN_TESTS_H
#define MOO_QUICK_OPEN_TESTS_H


#ifdef MOO_ENABLE_UNIT_TESTS


G_BEGIN_DECLS

/* Registers Quick Open's ranker unit tests; called from moo_unit_tests_run(). */
void        _moo_quick_open_add_unit_tests (void);

G_END_DECLS

#endif /* MOO_ENABLE_UNIT_TESTS */

#endif /* MOO_QUICK_OPEN_TESTS_H */
