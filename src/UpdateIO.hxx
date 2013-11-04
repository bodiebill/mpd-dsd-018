/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_UPDATE_IO_HXX
#define MPD_UPDATE_IO_HXX

#include "check.h"

#include <sys/stat.h>

struct Directory;

int
stat_directory(const Directory &directory, struct stat *st);

int
stat_directory_child(const Directory &parent, const char *name,
		     struct stat *st);

bool
directory_exists(const Directory &directory);

bool
directory_child_is_regular(const Directory &directory,
			   const char *name_utf8);

/**
 * Checks if the given permissions on the mapped file are given.
 */
bool
directory_child_access(const Directory &directory,
		       const char *name, int mode);

#endif
