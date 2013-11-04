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
#include "SoundCloudPlaylistPlugin.hxx"
#include "PlaylistPlugin.hxx"
#include "MemorySongEnumerator.hxx"
#include "ConfigData.hxx"
#include "InputStream.hxx"
#include "Song.hxx"
#include "tag/Tag.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>
#include <yajl/yajl_parse.h>

#include <string>

#include <string.h>

static struct {
	std::string apikey;
} soundcloud_config;

static constexpr Domain soundcloud_domain("soundcloud");

static bool
soundcloud_init(const config_param &param)
{
	soundcloud_config.apikey = param.GetBlockValue("apikey", "");
	if (soundcloud_config.apikey.empty()) {
		LogDebug(soundcloud_domain,
			 "disabling the soundcloud playlist plugin "
			 "because API key is not set");
		return false;
	}

	return true;
}

/**
 * Construct a full soundcloud resolver URL from the given fragment.
 * @param uri uri of a soundcloud page (or just the path)
 * @return Constructed URL. Must be freed with g_free.
 */
static char *
soundcloud_resolve(const char* uri) {
	char *u, *ru;

	if (g_str_has_prefix(uri, "http://")) {
		u = g_strdup(uri);
	} else if (g_str_has_prefix(uri, "soundcloud.com")) {
		u = g_strconcat("http://", uri, nullptr);
	} else {
		/* assume it's just a path on soundcloud.com */
		u = g_strconcat("http://soundcloud.com/", uri, nullptr);
	}

	ru = g_strconcat("http://api.soundcloud.com/resolve.json?url=",
			 u, "&client_id=",
			 soundcloud_config.apikey.c_str(), nullptr);
	g_free(u);

	return ru;
}

/* YAJL parser for track data from both /tracks/ and /playlists/ JSON */

enum key {
	Duration,
	Title,
	Stream_URL,
	Other,
};

const char* key_str[] = {
	"duration",
	"title",
	"stream_url",
	nullptr,
};

struct parse_data {
	int key;
	char* stream_url;
	long duration;
	char* title;
	int got_url; /* nesting level of last stream_url */

	std::forward_list<SongPointer> songs;
};

static int handle_integer(void *ctx,
			  long
#ifndef HAVE_YAJL1
			  long
#endif
			  intval)
{
	struct parse_data *data = (struct parse_data *) ctx;

	switch (data->key) {
	case Duration:
		data->duration = intval;
		break;
	default:
		break;
	}

	return 1;
}

static int handle_string(void *ctx, const unsigned char* stringval,
#ifdef HAVE_YAJL1
			 unsigned int
#else
			 size_t
#endif
			 stringlen)
{
	struct parse_data *data = (struct parse_data *) ctx;
	const char *s = (const char *) stringval;

	switch (data->key) {
	case Title:
		if (data->title != nullptr)
			g_free(data->title);
		data->title = g_strndup(s, stringlen);
		break;
	case Stream_URL:
		if (data->stream_url != nullptr)
			g_free(data->stream_url);
		data->stream_url = g_strndup(s, stringlen);
		data->got_url = 1;
		break;
	default:
		break;
	}

	return 1;
}

static int handle_mapkey(void *ctx, const unsigned char* stringval,
#ifdef HAVE_YAJL1
			 unsigned int
#else
			 size_t
#endif
			 stringlen)
{
	struct parse_data *data = (struct parse_data *) ctx;

	int i;
	data->key = Other;

	for (i = 0; i < Other; ++i) {
		if (memcmp((const char *)stringval, key_str[i], stringlen) == 0) {
			data->key = i;
			break;
		}
	}

	return 1;
}

static int handle_start_map(void *ctx)
{
	struct parse_data *data = (struct parse_data *) ctx;

	if (data->got_url > 0)
		data->got_url++;

	return 1;
}

static int handle_end_map(void *ctx)
{
	struct parse_data *data = (struct parse_data *) ctx;

	if (data->got_url > 1) {
		data->got_url--;
		return 1;
	}

	if (data->got_url == 0)
		return 1;

	/* got_url == 1, track finished, make it into a song */
	data->got_url = 0;

	Song *s;
	char *u;

	u = g_strconcat(data->stream_url, "?client_id=",
			soundcloud_config.apikey.c_str(), nullptr);
	s = Song::NewRemote(u);
	g_free(u);

	Tag *t = new Tag();
	t->time = data->duration / 1000;
	if (data->title != nullptr)
		t->AddItem(TAG_NAME, data->title);
	s->tag = t;

	data->songs.emplace_front(s);

	return 1;
}

static yajl_callbacks parse_callbacks = {
	nullptr,
	nullptr,
	handle_integer,
	nullptr,
	nullptr,
	handle_string,
	handle_start_map,
	handle_mapkey,
	handle_end_map,
	nullptr,
	nullptr,
};

/**
 * Read JSON data and parse it using the given YAJL parser.
 * @param url URL of the JSON data.
 * @param hand YAJL parser handle.
 * @return -1 on error, 0 on success.
 */
static int
soundcloud_parse_json(const char *url, yajl_handle hand,
		      Mutex &mutex, Cond &cond)
{
	char buffer[4096];
	unsigned char *ubuffer = (unsigned char *)buffer;

	Error error;
	InputStream *input_stream = InputStream::Open(url, mutex, cond,
						      error);
	if (input_stream == nullptr) {
		if (error.IsDefined())
			LogError(error);
		return -1;
	}

	mutex.lock();
	input_stream->WaitReady();

	yajl_status stat;
	int done = 0;

	while (!done) {
		const size_t nbytes =
			input_stream->Read(buffer, sizeof(buffer), error);
		if (nbytes == 0) {
			if (error.IsDefined())
				LogError(error);

			if (input_stream->IsEOF()) {
				done = true;
			} else {
				mutex.unlock();
				input_stream->Close();
				return -1;
			}
		}

		if (done) {
#ifdef HAVE_YAJL1
			stat = yajl_parse_complete(hand);
#else
			stat = yajl_complete_parse(hand);
#endif
		} else
			stat = yajl_parse(hand, ubuffer, nbytes);

		if (stat != yajl_status_ok
#ifdef HAVE_YAJL1
		    && stat != yajl_status_insufficient_data
#endif
		    )
		{
			unsigned char *str = yajl_get_error(hand, 1, ubuffer, nbytes);
			LogError(soundcloud_domain, (const char *)str);
			yajl_free_error(hand, str);
			break;
		}
	}

	mutex.unlock();
	input_stream->Close();

	return 0;
}

/**
 * Parse a soundcloud:// URL and create a playlist.
 * @param uri A soundcloud URL. Accepted forms:
 *	soundcloud://track/<track-id>
 *	soundcloud://playlist/<playlist-id>
 *	soundcloud://url/<url or path of soundcloud page>
 */

static SongEnumerator *
soundcloud_open_uri(const char *uri, Mutex &mutex, Cond &cond)
{
	char *s, *p;
	char *scheme, *arg, *rest;
	s = g_strdup(uri);
	scheme = s;
	for (p = s; *p; p++) {
		if (*p == ':' && *(p+1) == '/' && *(p+2) == '/') {
			*p = 0;
			p += 3;
			break;
		}
	}
	arg = p;
	for (; *p; p++) {
		if (*p == '/') {
			*p = 0;
			p++;
			break;
		}
	}
	rest = p;

	if (strcmp(scheme, "soundcloud") != 0) {
		FormatWarning(soundcloud_domain,
			      "incompatible scheme for soundcloud plugin: %s",
			      scheme);
		g_free(s);
		return nullptr;
	}

	char *u = nullptr;
	if (strcmp(arg, "track") == 0) {
		u = g_strconcat("http://api.soundcloud.com/tracks/",
				rest, ".json?client_id=",
				soundcloud_config.apikey.c_str(), nullptr);
	} else if (strcmp(arg, "playlist") == 0) {
		u = g_strconcat("http://api.soundcloud.com/playlists/",
				rest, ".json?client_id=",
				soundcloud_config.apikey.c_str(), nullptr);
	} else if (strcmp(arg, "url") == 0) {
		/* Translate to soundcloud resolver call. libcurl will automatically
		   follow the redirect to the right resource. */
		u = soundcloud_resolve(rest);
	}
	g_free(s);

	if (u == nullptr) {
		LogWarning(soundcloud_domain, "unknown soundcloud URI");
		return nullptr;
	}

	yajl_handle hand;
	struct parse_data data;

	data.got_url = 0;
	data.title = nullptr;
	data.stream_url = nullptr;
#ifdef HAVE_YAJL1
	hand = yajl_alloc(&parse_callbacks, nullptr, nullptr, (void *) &data);
#else
	hand = yajl_alloc(&parse_callbacks, nullptr, (void *) &data);
#endif

	int ret = soundcloud_parse_json(u, hand, mutex, cond);

	g_free(u);
	yajl_free(hand);
	if (data.title != nullptr)
		g_free(data.title);
	if (data.stream_url != nullptr)
		g_free(data.stream_url);

	if (ret == -1)
		return nullptr;

	data.songs.reverse();
	return new MemorySongEnumerator(std::move(data.songs));
}

static const char *const soundcloud_schemes[] = {
	"soundcloud",
	nullptr
};

const struct playlist_plugin soundcloud_playlist_plugin = {
	"soundcloud",

	soundcloud_init,
	nullptr,
	soundcloud_open_uri,
	nullptr,

	soundcloud_schemes,
	nullptr,
	nullptr,
};


