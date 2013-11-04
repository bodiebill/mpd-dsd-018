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
#include "FileCommands.hxx"
#include "CommandError.hxx"
#include "protocol/Ack.hxx"
#include "protocol/Result.hxx"
#include "ClientFile.hxx"
#include "Client.hxx"
#include "util/CharUtil.hxx"
#include "util/Error.hxx"
#include "tag/TagHandler.hxx"
#include "tag/ApeTag.hxx"
#include "tag/TagId3.hxx"
#include "TagFile.hxx"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"

#include <assert.h>

gcc_pure
static bool
IsValidName(const char *p)
{
	if (!IsAlphaASCII(*p))
		return false;

	while (*++p) {
		const char ch = *p;
		if (!IsAlphaASCII(ch) && ch != '_' && ch != '-')
			return false;
	}

	return true;
}

gcc_pure
static bool
IsValidValue(const char *p)
{
	while (*p) {
		const char ch = *p++;

		if ((unsigned char)ch < 0x20)
			return false;
	}

	return true;
}

static void
print_pair(const char *key, const char *value, void *ctx)
{
	Client &client = *(Client *)ctx;

	if (IsValidName(key) && IsValidValue(value))
		client_printf(client, "%s: %s\n", key, value);
}

static constexpr tag_handler print_comment_handler = {
	nullptr,
	nullptr,
	print_pair,
};

CommandResult
handle_read_comments(Client &client, gcc_unused int argc, char *argv[])
{
	assert(argc == 2);

	const char *const uri = argv[1];

	AllocatedPath path_fs = AllocatedPath::Null();

	if (memcmp(uri, "file:///", 8) == 0) {
		/* read comments from arbitrary local file */
		const char *path_utf8 = uri + 7;
		path_fs = AllocatedPath::FromUTF8(path_utf8);
		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return CommandResult::ERROR;
		}

		Error error;
		if (!client_allow_file(client, path_fs, error))
			return print_error(client, error);
	} else if (*uri != '/') {
		path_fs = map_uri_fs(uri);
		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "No such file");
			return CommandResult::ERROR;
		}
	} else {
		command_error(client, ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
	}

	if (!tag_file_scan(path_fs, &print_comment_handler, &client)) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "Failed to load file");
		return CommandResult::ERROR;
	}

	tag_ape_scan2(path_fs, &print_comment_handler, &client);
	tag_id3_scan(path_fs, &print_comment_handler, &client);

	return CommandResult::OK;
}
