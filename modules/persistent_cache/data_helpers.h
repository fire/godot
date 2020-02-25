/*************************************************************************/
/*  data_helpers.h                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef CACHE_INFO_TABLE_H
#define CACHE_INFO_TABLE_H

#include "core/map.h"
#include "core/object.h"
#include "core/os/file_access.h"
#include "core/os/rw_lock.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/reference.h"
#include "core/rid.h"
#include "core/set.h"
#include "core/variant.h"
#include "core/vector.h"

#include "cacheserv_defines.h"

// Int to hex string.
_FORCE_INLINE_ String itoh(size_t num) {
	char x[100];
	snprintf(x, 100, "0x%lX", (unsigned long)num);
	return String(x);
}

typedef uint32_t data_descriptor;
typedef uint32_t frame_id;
typedef uint64_t page_id;

struct CacheInfoTable;
struct Frame;
struct DescriptorInfo;

class FileCacheManager;

struct DescriptorInfo {
	String path;
	Vector<page_id> pages;
	FileAccess *internal_data_source;
	Semaphore *ready_sem;
	Semaphore *dirty_sem;
	RWLock *lock;
	size_t offset;
	size_t total_size;
	page_id guid_prefix;
	int cache_policy;
	int max_pages;
	bool valid;
	bool dirty;

	// Create a new DescriptorInfo with a new random namespace defined by 24 most significant bits.
	DescriptorInfo(FileAccess *fa, page_id new_guid_prefix, int cache_policy);
	~DescriptorInfo() {
		while (dirty) dirty_sem->wait();
		memdelete(ready_sem);
		memdelete(dirty_sem);
		memdelete(lock);
	}

	Variant to_variant(const FileCacheManager &p);
};

struct Frame {
	friend class FileCacheManager;

private:
	uint8_t *const memory_region;
	page_id owning_page;
	uint32_t ts_last_use;
	uint16_t used_size;
	volatile bool dirty;
	volatile bool ready;
	volatile bool used;

public:
	Frame() :
			memory_region(NULL),
			owning_page(0),
			ts_last_use(0),
			used_size(0),
			dirty(false),
			ready(false),
			used(false) {}

	explicit Frame(
			uint8_t *i_memory_region) :

			memory_region(i_memory_region),
			owning_page(0),
			ts_last_use(0),
			used_size(0),
			dirty(false),
			ready(false),
			used(false) {}

	~Frame() {
	}

	_FORCE_INLINE_ page_id get_owning_page() {
		return owning_page;
	}

	_FORCE_INLINE_ Frame &set_owning_page(page_id page) {
		// A frame whose owning page is changing should not be dirty and should be in a non-ready state.
		CRASH_COND(dirty || ready)
		owning_page = page;
		return *this;
	}

	_FORCE_INLINE_ bool get_dirty() {
		return dirty;
	}

	_FORCE_INLINE_ Frame &set_dirty_true() {
		// A page that isn't ready can't become dirty.
		CRASH_COND(!ready)
		dirty = true;
		String a;
		a.parse_utf8((const char *)(this->memory_region), 4095);
		// WARN_PRINTS("Dirty page.\n\n" + a + "\n\n");
		return *this;
	}

	_FORCE_INLINE_ Frame &set_dirty_false(Semaphore *dirty_sem, frame_id frame) {
		// A page which is dirty as well as not ready is in an invalid state.
		CRASH_COND(!ready)
		dirty = false;
		// WARN_PRINTS("Dirty page " + itoh(frame) + " is clean.");
		dirty_sem->post();
		return *this;
	}

	_FORCE_INLINE_ bool get_used() {
		return used;
	}

	_FORCE_INLINE_ Frame &set_used(bool in) {
		// All io ops must be completed (page must not be dirty) for this transition to be valid.
		CRASH_COND(dirty)
		used = in;
		return *this;
	}

	_FORCE_INLINE_ bool get_ready() {
		return ready;
	}

	_FORCE_INLINE_ Frame &set_ready_true(Semaphore *ready_sem) {
		// A page cannot be dirty before it is ready.
		CRASH_COND(!ready && dirty)
		ready = true;
		ready_sem->post();
		// WARN_PRINTS("Part ready for page " + itoh(page) + " and frame " + itoh(frame) + " .");
		return *this;
	}

	_FORCE_INLINE_ Frame &set_ready_false() {
		// A page that is dirty must always be ready.
		CRASH_COND(dirty)
		ready = false;
		return *this;
	}

	_FORCE_INLINE_ uint32_t get_last_use() {
		return ts_last_use;
	}

	_FORCE_INLINE_ Frame &set_last_use(uint32_t in) {
		// maybe unnecessary.
		// CRASH_COND(dirty)
		ts_last_use = in;
		return *this;
	}

	_FORCE_INLINE_ Frame &wait_clean(Semaphore *sem) {
		while (dirty != false)
			sem->wait();
		// ERR_PRINTS("Page is clean.")
		return *this;
	}

	_FORCE_INLINE_ Frame &wait_ready(Semaphore *sem) {
		while (ready != true)
			sem->wait();
		// ERR_PRINTS("Page is clean.")
		return *this;
	}

	_FORCE_INLINE_ uint16_t get_used_size() {
		return used_size;
	}

	_FORCE_INLINE_ Frame &set_used_size(uint16_t in) {
		used_size = in;
		return *this;
	}

	Variant to_variant() const {
		Dictionary a;
		char s[101] = {0};
		memcpy(s, memory_region, 100);

		a["memory_region"] = Variant(itoh(reinterpret_cast<size_t>(memory_region)) +  " # " + s + " ... ");
		a["used_size"] = Variant(itoh(used_size));
		a["time_since_last_use"] = Variant(itoh(ts_last_use));
		a["used"] = Variant(used);
		a["dirty"] = Variant(dirty);
		a["ready"] = Variant(ready);

		return Variant(a);
	}

	class DataRead {
	private:
		RWLock *rwl;
		const uint8_t *mem;

	public:
		_FORCE_INLINE_ const uint8_t &operator[](int p_index) const { return mem[p_index]; }
		_FORCE_INLINE_ const uint8_t *ptr() const { return mem; }

		void acquire() {
			rwl->read_lock();
		}

		DataRead() :
				rwl(NULL),
				mem(NULL) {}

		DataRead(const Frame *alloc, DescriptorInfo *desc_info) :
				rwl(desc_info->lock),
				mem(alloc->memory_region) {
			while (!(alloc->ready))
				desc_info->ready_sem->wait();
			// WARN_PRINT(("Acquiring data READ lock in thread ID "  + itoh(Thread::get_caller_id()) ).utf8().get_data());
			acquire();
		}

		~DataRead() {
			if (rwl) {
				rwl->read_unlock();
				// WARN_PRINT(("Releasing data READ lock in thread ID " + itoh(Thread::get_caller_id())).utf8().get_data());
			}
		}
	};

	class DataWrite {
	private:
		RWLock *rwl;
		uint8_t *mem;

	public:
		_FORCE_INLINE_ uint8_t &operator[](int p_index) const { return mem[p_index]; }
		_FORCE_INLINE_ uint8_t *ptr() const { return mem; }

		void acquire() {
			// WARN_PRINT(("Acquiring data WRITE lock in thread ID " + itoh(Thread::get_caller_id())).utf8().get_data());
			rwl->write_lock();
		}

		DataWrite() :
				rwl(NULL),
				mem(NULL) {}

		// We must wait for the page to become clean if we want to write to this page from a file. But, if we're writing from the main thread, we can safely allow this operation to occur.
		DataWrite(Frame *const p_alloc, DescriptorInfo *desc_info, bool is_io_op) :
				rwl(desc_info->lock),
				mem(p_alloc->memory_region) {
			if (is_io_op)
				while (p_alloc->dirty)
					desc_info->dirty_sem->wait();
			acquire();
		}

		~DataWrite() {
			if (rwl) {
				rwl->write_unlock();
				// WARN_PRINT(("Releasing data WRITE lock in thread ID " + itoh(Thread::get_caller_id())).utf8().get_data());
			}
		}
	};
};

#endif // !CACHE_INFO_TABLE_H
