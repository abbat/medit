/*
 *   mooutils/mooutils-tests.h
 *
 *   Copyright (C) 2023-2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 */

#ifndef MOO_UTILS_TESTS_H
#define MOO_UTILS_TESTS_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef MOO_ENABLE_UNIT_TESTS

#include <glib.h>

G_BEGIN_DECLS

/* Registers the mooutils tests; called from moo_unit_tests_run(). */
void        _moo_add_mooutils_unit_tests    (void);

G_END_DECLS

#endif /* MOO_ENABLE_UNIT_TESTS */

#endif /* MOO_UTILS_TESTS_H */
