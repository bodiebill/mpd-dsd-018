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
#include "InputStream.hxx"
#include "InputRegistry.hxx"
#include "InputPlugin.hxx"
#include "input/RewindInputPlugin.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <assert.h>
#include <stdio.h> /* for SEEK_SET */

static constexpr Domain input_domain("input");

InputStream *
InputStream::Open(const char *url,
		  Mutex &mutex, Cond &cond,
		  Error &error)
{
	input_plugins_for_each_enabled(plugin) {
		InputStream *is;

		is = plugin->open(url, mutex, cond, error);
		if (is != nullptr) {
			assert(is->plugin.close != nullptr);
			assert(is->plugin.read != nullptr);
			assert(is->plugin.eof != nullptr);
			assert(!is->seekable || is->plugin.seek != nullptr);

			is = input_rewind_open(is);

			return is;
		} else if (error.IsDefined())
			return nullptr;
	}

	error.Set(input_domain, "Unrecognized URI");
	return nullptr;
}

bool
InputStream::Check(Error &error)
{
	return plugin.check == nullptr || plugin.check(this, error);
}

void
InputStream::Update()
{
	if (plugin.update != nullptr)
		plugin.update(this);
}

void
InputStream::WaitReady()
{
	while (true) {
		Update();
		if (ready)
			break;

		cond.wait(mutex);
	}
}

void
InputStream::LockWaitReady()
{
	const ScopeLock protect(mutex);
	WaitReady();
}

bool
InputStream::CheapSeeking() const
{
	return IsSeekable() && !uri_has_scheme(uri.c_str());
}

bool
InputStream::Seek(offset_type _offset, int whence, Error &error)
{
	if (plugin.seek == nullptr)
		return false;

	return plugin.seek(this, _offset, whence, error);
}

bool
InputStream::LockSeek(offset_type _offset, int whence, Error &error)
{
	if (plugin.seek == nullptr)
		return false;

	const ScopeLock protect(mutex);
	return Seek(_offset, whence, error);
}

bool
InputStream::Rewind(Error &error)
{
	return Seek(0, SEEK_SET, error);
}

bool
InputStream::LockRewind(Error &error)
{
	return LockSeek(0, SEEK_SET, error);
}

Tag *
InputStream::ReadTag()
{
	return plugin.tag != nullptr
		? plugin.tag(this)
		: nullptr;
}

Tag *
InputStream::LockReadTag()
{
	if (plugin.tag == nullptr)
		return nullptr;

	const ScopeLock protect(mutex);
	return ReadTag();
}

bool
InputStream::IsAvailable()
{
	return plugin.available != nullptr
		? plugin.available(this)
		: true;
}

size_t
InputStream::Read(void *ptr, size_t _size, Error &error)
{
	assert(ptr != nullptr);
	assert(_size > 0);

	return plugin.read(this, ptr, _size, error);
}

size_t
InputStream::LockRead(void *ptr, size_t _size, Error &error)
{
	assert(ptr != nullptr);
	assert(_size > 0);

	const ScopeLock protect(mutex);
	return Read(ptr, _size, error);
}

void
InputStream::Close()
{
	plugin.close(this);
}

bool
InputStream::IsEOF()
{
	return plugin.eof(this);
}

bool
InputStream::LockIsEOF()
{
	const ScopeLock protect(mutex);
	return IsEOF();
}

