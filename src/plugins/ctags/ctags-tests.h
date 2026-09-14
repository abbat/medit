/*
 *   ctags/ctags-tests.h
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#ifndef CTAGS_TESTS_H
#define CTAGS_TESTS_H


#ifdef MOO_ENABLE_UNIT_TESTS


G_BEGIN_DECLS

/* Registers the ctags plugin tests; called from moo_unit_tests_run(). */
void        _moo_add_ctags_unit_tests (void);

G_END_DECLS

#endif /* MOO_ENABLE_UNIT_TESTS */

#endif /* CTAGS_TESTS_H */
