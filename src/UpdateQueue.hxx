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

#ifndef MPD_UPDATE_QUEUE_HXX
#define MPD_UPDATE_QUEUE_HXX

#include "check.h"

#include <string>

struct UpdateQueueItem {
	std::string path_utf8;
	unsigned id;
	bool discard;

	UpdateQueueItem():id(0) {}
	UpdateQueueItem(const char *_path, bool _discard,
			unsigned _id)
		:path_utf8(_path), id(_id), discard(_discard) {}

	bool IsDefined() const {
		return id != 0;
	}
};

bool
update_queue_push(const char *path, bool discard, unsigned id);

UpdateQueueItem
update_queue_shift();

#endif
