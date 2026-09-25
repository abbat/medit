/*
 *   medit-app/main.h
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

#pragma once
#ifndef _medit_app_main_h_
#define _medit_app_main_h_

#include "sysheaders.h"

G_BEGIN_DECLS;

/*!
 * \brief Main function of the medit application
 *
 * Performs application initialization, command line argument parsing,
 * file processing, launching a new instance or connecting to an existing one.
 *
 * \param argc Number of command line arguments
 * \param argv Array of command line arguments
 * \return Application exit code
 */
int
medit_app_main (int argc, char *argv[]);

#ifdef MOO_ENABLE_UNIT_TESTS
/* Registers the medit-app tests (parse_options_from_uri); called from
   moo_unit_tests_run(). */
void
_moo_add_main_unit_tests (void);
#endif

G_END_DECLS;

#endif /* _medit_app_main_h_ */
