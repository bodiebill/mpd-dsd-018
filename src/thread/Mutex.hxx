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

#ifndef MPD_THREAD_MUTEX_HXX
#define MPD_THREAD_MUTEX_HXX

#ifdef WIN32

/* mingw-w64 4.6.3 lacks a std::mutex implementation */

#include "CriticalSection.hxx"
class Mutex : public CriticalSection {};

#else

#include "PosixMutex.hxx"
class Mutex : public PosixMutex {};

#endif

class ScopeLock {
	Mutex &mutex;

public:
	ScopeLock(Mutex &_mutex):mutex(_mutex) {
		mutex.lock();
	};

	~ScopeLock() {
		mutex.unlock();
	};

	ScopeLock(const ScopeLock &other) = delete;
	ScopeLock &operator=(const ScopeLock &other) = delete;
};

#endif
