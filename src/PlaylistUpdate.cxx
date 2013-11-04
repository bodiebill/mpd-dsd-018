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
#include "Playlist.hxx"
#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"
#include "Song.hxx"
#include "tag/Tag.hxx"
#include "Idle.hxx"
#include "util/Error.hxx"

static bool
UpdatePlaylistSong(const Database &db, Song &song)
{
	if (!song.IsInDatabase() || !song.IsDetached())
		/* only update Songs instances that are "detached"
		   from the Database */
		return false;

	Song *original = db.GetSong(song.uri, IgnoreError());
	if (original == nullptr)
		/* not found - shouldn't happen, because the update
		   thread should ensure that all stale Song instances
		   have been purged */
		return false;

	if (original->mtime == song.mtime) {
		/* not modified */
		db.ReturnSong(original);
		return false;
	}

	song.mtime = original->mtime;

	if (original->tag != nullptr)
		song.ReplaceTag(Tag(*original->tag));

	db.ReturnSong(original);
	return true;
}

void
playlist::DatabaseModified()
{
	const Database *db = GetDatabase();
	if (db == nullptr)
		/* how can this ever happen? */
		return;

	bool modified = false;

	for (unsigned i = 0, n = queue.GetLength(); i != n; ++i) {
		if (UpdatePlaylistSong(*db, queue.Get(i))) {
			queue.ModifyAtPosition(i);
			modified = true;
		}
	}

	if (modified) {
		queue.IncrementVersion();
		idle_add(IDLE_PLAYLIST);
	}
}
