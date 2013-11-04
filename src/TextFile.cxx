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

#include "config.h"
#include "TextFile.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

TextFile::TextFile(Path path_fs)
	:file(FOpen(path_fs, FOpenMode::ReadText)),
	 buffer(g_string_sized_new(step)) {}

TextFile::~TextFile()
{
	if (file != nullptr)
		fclose(file);

	g_string_free(buffer, true);
}

char *
TextFile::ReadLine()
{
	gsize length = 0, i;
	char *p;

	assert(file != nullptr);
	assert(buffer != nullptr);
	assert(buffer->allocated_len >= step);

	while (buffer->len < max_length) {
		p = fgets(buffer->str + length,
			  buffer->allocated_len - length, file);
		if (p == nullptr) {
			if (length == 0 || ferror(file))
				return nullptr;
			break;
		}

		i = strlen(buffer->str + length);
		length += i;
		if (i < step - 1 || buffer->str[length - 1] == '\n')
			break;

		g_string_set_size(buffer, length + step);
	}

	/* remove the newline characters */
	if (buffer->str[length - 1] == '\n')
		--length;
	if (buffer->str[length - 1] == '\r')
		--length;

	g_string_set_size(buffer, length);
	return buffer->str;
}
