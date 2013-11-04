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

#ifndef MPD_FS_TRAITS_HXX
#define MPD_FS_TRAITS_HXX

#include "check.h"
#include "Compiler.h"

#ifdef WIN32
#include <glib.h>
#endif

#include <string>

#include <assert.h>

class Error;

/**
 * This class describes the nature of a filesystem path.
 */
struct PathTraits {
	typedef char value_type;
	typedef char *pointer;
	typedef const char *const_pointer;

#ifdef WIN32
	static constexpr value_type SEPARATOR_FS = '\\';
	static constexpr char SEPARATOR_UTF8 = '/';
#else
	static constexpr value_type SEPARATOR_FS = '/';
	static constexpr char SEPARATOR_UTF8 = '/';
#endif

	static constexpr bool IsSeparatorFS(value_type ch) {
		return
#ifdef WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR_FS;
	}

	static constexpr bool IsSeparatorUTF8(char ch) {
		return
#ifdef WIN32
			ch == '/' ||
#endif
			ch == SEPARATOR_UTF8;
	}

	gcc_pure
	static bool IsAbsoluteFS(const_pointer p) {
		assert(p != nullptr);

#ifdef WIN32
		return g_path_is_absolute(p);
#else
		return IsSeparatorFS(*p);
#endif
	}

	gcc_pure
	static bool IsAbsoluteUTF8(const char *p) {
		assert(p != nullptr);

#ifdef WIN32
		return g_path_is_absolute(p);
#else
		return IsSeparatorUTF8(*p);
#endif
	}

	/**
	 * Determine the "base" file name of the given UTF-8 path.
	 * The return value points inside the given string.
	 */
	gcc_pure gcc_nonnull_all
	static const char *GetBaseUTF8(const char *p);

	/**
	 * Determine the "parent" file name of the given UTF-8 path.
	 * As a special case, returns the string "." if there is no
	 * separator in the given input string.
	 */
	gcc_pure gcc_nonnull_all
	static std::string GetParentUTF8(const char *p);
};

#endif
