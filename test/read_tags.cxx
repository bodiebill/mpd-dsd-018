/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "IOThread.hxx"
#include "DecoderList.hxx"
#include "DecoderPlugin.hxx"
#include "InputInit.hxx"
#include "InputStream.hxx"
#include "AudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagId3.hxx"
#include "tag/ApeTag.hxx"
#include "util/Error.hxx"
#include "fs/Path.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

static bool empty = true;

static void
print_duration(unsigned seconds, gcc_unused void *ctx)
{
	g_print("duration=%d\n", seconds);
}

static void
print_tag(TagType type, const char *value, gcc_unused void *ctx)
{
	g_print("[%s]=%s\n", tag_item_names[type], value);
	empty = false;
}

static void
print_pair(const char *name, const char *value, gcc_unused void *ctx)
{
	g_print("\"%s\"=%s\n", name, value);
}

static const struct tag_handler print_handler = {
	print_duration,
	print_tag,
	print_pair,
};

int main(int argc, char **argv)
{
	const char *decoder_name, *path;
	const struct DecoderPlugin *plugin;

#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	if (argc != 3) {
		g_printerr("Usage: read_tags DECODER FILE\n");
		return 1;
	}

	decoder_name = argv[1];
	path = argv[2];

#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif

	io_thread_init();
	io_thread_start();

	Error error;
	if (!input_stream_global_init(error)) {
		LogError(error);
		return 2;
	}

	decoder_plugin_init_all();

	plugin = decoder_plugin_from_name(decoder_name);
	if (plugin == NULL) {
		g_printerr("No such decoder: %s\n", decoder_name);
		return 1;
	}

	bool success = plugin->ScanFile(path, print_handler, nullptr);
	if (!success && plugin->scan_stream != NULL) {
		Mutex mutex;
		Cond cond;

		InputStream *is = InputStream::Open(path, mutex, cond,
						    error);
		if (is == NULL) {
			g_printerr("Failed to open %s: %s\n",
				   path, error.GetMessage());
			return 1;
		}

		mutex.lock();

		is->WaitReady();

		if (!is->Check(error)) {
			mutex.unlock();

			g_printerr("Failed to read %s: %s\n",
				   path, error.GetMessage());
			return EXIT_FAILURE;
		}

		mutex.unlock();

		success = plugin->ScanStream(*is, print_handler, nullptr);
		is->Close();
	}

	decoder_plugin_deinit_all();
	input_stream_global_finish();
	io_thread_deinit();

	if (!success) {
		g_printerr("Failed to read tags\n");
		return 1;
	}

	if (empty) {
		tag_ape_scan2(Path::FromFS(path), &print_handler, NULL);
		if (empty)
			tag_id3_scan(Path::FromFS(path), &print_handler, NULL);
	}

	return 0;
}
