/*************************************************************************/
/*  file_access_cached.h                                                 */
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

#ifndef FILE_ACCESS_CACHED_H
#define FILE_ACCESS_CACHED_H

#include "core/engine.h"
#include "core/io/marshalls.h"
#include "core/object.h"
#include "core/os/file_access.h"
#include "core/os/semaphore.h"

#include "file_cache_manager.h"

class _FileAccessCached;

class FileAccessCached : public FileAccess, public Object {
	GDCLASS(FileAccessCached, Object);

	friend class _FileAccessCached;
	friend class FileCacheManager;

	static FileAccess *create() { return (FileAccess *)memnew(FileAccessCached); }

private:
	String rel_path;
	String abs_path;

	Error last_error;

	FileCacheManager *cache_mgr;
	RID cached_file;
	Semaphore *sem;

protected:
	Error cached_open(const String &p_path, int p_mode_flags, int cache_policy) {
		cached_file = cache_mgr->open(p_path, p_mode_flags, cache_policy);
		ERR_FAIL_COND_V(cached_file.is_valid() == false, ERR_CANT_OPEN);
		return OK;
	}

	// Can't really use this.
	virtual Error _open(const String &p_path, int p_mode_flags) {
		return ERR_UNAVAILABLE;
	} ///< open a file

	template <typename T>
	_FORCE_INLINE_ T get_t() const {

		T buf = CS_MEM_VAL_BAD;
		cache_mgr->check_cache(cached_file, sizeof(T));
		size_t o_length = cache_mgr->read(cached_file, &buf, sizeof(T));
		if (o_length < sizeof(T)) {
			ERR_PRINTS("Read less than " + itos(sizeof(T)) + " byte(s).");
		}
		return buf;
	}

	template <typename T>
	void store_t(T buf) {

		cache_mgr->check_cache(cached_file, sizeof(T));
		size_t o_length = cache_mgr->write(cached_file, &buf, sizeof(T));
		if (o_length < sizeof(T)) {
			ERR_PRINTS("Read less than " + itos(sizeof(T)) + " byte(s).");
		}
	}

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("open", "path", "mode"), &FileAccessCached::_open);
		ClassDB::bind_method(D_METHOD("close"), &FileAccessCached::close);
		//ClassDB::bind_method(D_METHOD("get_buffer", "len"), &FileAccessCached::get_buffer);
		ClassDB::bind_method(D_METHOD("seek", "position"), &FileAccessCached::seek);
		ClassDB::bind_method(D_METHOD("seek_end", "position"), &FileAccessCached::seek_end);
		ClassDB::bind_method(D_METHOD("save_cache", "path"), &FileAccessCached::save_cache);
	}

public:

	virtual uint32_t _get_unix_permissions(const String &p_file) { return FileAccess::get_unix_permissions(p_file); }
	virtual Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) { return FileAccess::set_unix_permissions(p_file, p_permissions); }

	void save_cache(NodePath p_path) {
		ResourceSaver::save(p_path, cache_mgr);
	}

	void close() {
		if (cached_file.is_valid()) {
			cache_mgr->close(cached_file);
		}
	} ///< close a file

	// Completely removes the file from the cache, including cached pages.
	void permanent_close() {
		if (cached_file.is_valid()) {
			cache_mgr->close(cached_file);
			cache_mgr->permanent_close(cached_file);
			cached_file = RID();
		}
	}

	virtual bool is_open() const { return this->cached_file.is_valid(); } ///< true when file is open

	virtual String get_path() const { return rel_path; } /// returns the path for the current open file
	virtual String get_path_absolute() const { return abs_path; } /// returns the absolute path for the current open file

	virtual void seek(size_t p_position) {
		cache_mgr->seek(cached_file, p_position);
		// After we seek, we check that the data there exists in the cache.
		cache_mgr->check_cache(cached_file, CS_LEN_UNSPECIFIED);
	} ///< seek to a given position

	virtual void seek_end(int64_t p_position) { cache_mgr->seek_end(cached_file, p_position); } ///< seek from the end of file

	virtual size_t get_position() const { return cache_mgr->get_len(cached_file); } ///< get position in the file

	virtual size_t get_len() const { return cache_mgr->get_len(cached_file); } ///< get size of the file

	virtual bool eof_reached() const { return cache_mgr->eof_reached(cached_file); } ///< reading passed EOF

	virtual uint8_t get_8() const { return get_t<uint8_t>(); } ///< get a byte

	virtual int get_buffer(uint8_t *p_dst, int p_length) const {
		int o_length = 0;
		//ERR_PRINTS("Initial offset: " + itoh(cache_mgr->get_position(cached_file)));

		// Read 4 pages of data at a time. This is so that it will always be
		// possible to have pages of a file present in the cache without worrying
		// about not being able to add new pages to the cache or reading from invalid pages.

		// This reads blocks of bytes at a time rather than calling get_8 a bunch of times.

		for (int i = 0; i < p_length - (p_length % (CS_PAGE_SIZE * 4)); i += CS_PAGE_SIZE * 2) {
			//ERR_PRINTS("Current offset: " + itoh(cache_mgr->get_position(cached_file) + i));
			cache_mgr->check_cache(cached_file, CS_PAGE_SIZE * 4);
			o_length += cache_mgr->read(cached_file, p_dst + i, CS_PAGE_SIZE * 2);
		}

		if ((p_length % (CS_PAGE_SIZE * 4)) > 0) {
			//ERR_PRINTS("Current offset: " + itoh(cache_mgr->get_position(cached_file)));
			cache_mgr->check_cache(cached_file, CS_PAGE_SIZE * 4);
			o_length += cache_mgr->read(cached_file, p_dst + (p_length - (p_length % (CS_PAGE_SIZE * 4))), (p_length % (4 * CS_PAGE_SIZE)));
		}
		if (p_length > o_length) {
			ERR_PRINTS("Read less than " + itos(p_length) + " bytes.\n");
		}
		return o_length;
	} ///< get an array of bytes

	virtual Error get_error() const { return last_error; } ///< get last error

	virtual void flush() { cache_mgr->flush(cached_file); }

	virtual void store_8(uint8_t p_dest) { return store_t(p_dest); } ///< store a byte

	virtual void store_buffer(const uint8_t *p_src, int p_length) {

		int o_length = 0;

		for (int i = 0; i < p_length - (p_length % (CS_PAGE_SIZE * 4)); i += CS_PAGE_SIZE * 2) {
			//ERR_PRINTS("Current offset: " + itoh(cache_mgr->get_position(cached_file) + i));
			cache_mgr->check_cache(cached_file, CS_PAGE_SIZE * 4);
			o_length += cache_mgr->write(cached_file, p_src + i, CS_PAGE_SIZE * 2);
		}

		if ((p_length % (CS_PAGE_SIZE * 4)) > 0) {
			//ERR_PRINTS("Current offset: " + itoh(cache_mgr->get_position(cached_file)));
			cache_mgr->check_cache(cached_file, CS_PAGE_SIZE * 4);
			o_length += cache_mgr->write(cached_file, p_src + (p_length - (p_length % (CS_PAGE_SIZE * 4))), (p_length % (4 * CS_PAGE_SIZE)));
		}

		if (p_length > o_length) {
			ERR_PRINTS("Wrote less than " + itos(p_length) + " bytes.\n");
		}
	} ///< store an array of bytes

	bool file_exists(const String &p_name) { return cache_mgr->file_exists(p_name); } ///< return true if a file exists

	uint64_t _get_modified_time(const String &p_file) { return FileAccess::get_modified_time(p_file); }

	Error _chmod(const String &p_path, int p_mod) { return ERR_UNAVAILABLE; }

	Error reopen(const String &p_path, int p_mode_flags) { return ERR_UNAVAILABLE; } ///< does not change the AccessType

	FileAccessCached() {

		cache_mgr = FileCacheManager::get_singleton();
		CRASH_COND(!cache_mgr);
		sem = Semaphore::create();
		CRASH_COND(!sem);
	}

	virtual ~FileAccessCached() {
		//WARN_PRINT("FileAccesCached destructor");
		close();
		memdelete(sem);
	}
};

class _FileAccessCached : public Object {

	GDCLASS(_FileAccessCached, Object);

	friend class FileAccessCached;

protected:
	FileAccessCached fac;

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("open", "path", "mode", "cache_policy"), &_FileAccessCached::open);
		ClassDB::bind_method(D_METHOD("close"), &_FileAccessCached::close);

		ClassDB::bind_method(D_METHOD("get_8"), &_FileAccessCached::get_8);
		ClassDB::bind_method(D_METHOD("get_16"), &_FileAccessCached::get_16);
		ClassDB::bind_method(D_METHOD("get_32"), &_FileAccessCached::get_32);
		ClassDB::bind_method(D_METHOD("get_64"), &_FileAccessCached::get_64);

		ClassDB::bind_method(D_METHOD("get_float"), &_FileAccessCached::get_float);
		ClassDB::bind_method(D_METHOD("get_double"), &_FileAccessCached::get_double);
		ClassDB::bind_method(D_METHOD("get_real"), &_FileAccessCached::get_real);

		ClassDB::bind_method(D_METHOD("get_buffer", "len"), &_FileAccessCached::get_buffer);
		ClassDB::bind_method(D_METHOD("get_line"), &_FileAccessCached::get_line);
		ClassDB::bind_method(D_METHOD("get_csv_line"), &_FileAccessCached::get_csv_line);

		ClassDB::bind_method(D_METHOD("store_8"), &_FileAccessCached::store_8);
		ClassDB::bind_method(D_METHOD("store_16"), &_FileAccessCached::store_16);
		ClassDB::bind_method(D_METHOD("store_32"), &_FileAccessCached::store_32);
		ClassDB::bind_method(D_METHOD("store_64"), &_FileAccessCached::store_64);

		ClassDB::bind_method(D_METHOD("store_float"), &_FileAccessCached::store_float);
		ClassDB::bind_method(D_METHOD("store_double"), &_FileAccessCached::store_double);
		ClassDB::bind_method(D_METHOD("store_real"), &_FileAccessCached::store_real);

		ClassDB::bind_method(D_METHOD("store_buffer"), &_FileAccessCached::store_buffer);
		ClassDB::bind_method(D_METHOD("store_line"), &_FileAccessCached::store_line);
		ClassDB::bind_method(D_METHOD("store_csv_line"), &_FileAccessCached::store_csv_line);

		ClassDB::bind_method(D_METHOD("seek", "position"), &_FileAccessCached::seek);
		ClassDB::bind_method(D_METHOD("seek_end", "position"), &_FileAccessCached::seek_end, DEFVAL(0));

		ClassDB::bind_method(D_METHOD("eof_reached"), &_FileAccessCached::eof_reached);
		ClassDB::bind_method(D_METHOD("flush"), &_FileAccessCached::flush);
		ClassDB::bind_method(D_METHOD("save_cache", "path"), &_FileAccessCached::save_cache);
	}

public:
	_FileAccessCached() {}
	~_FileAccessCached() {}

	bool eof_reached() { return fac.eof_reached(); }

	Variant open(String path, int mode, int cache_policy) {

		if (fac.cached_open(path, mode, cache_policy) == OK) {
			return this;
		} else
			return Variant();
	}

	uint8_t get_8() { return fac.get_8(); }

	uint16_t get_16() { return fac.get_16(); }

	uint32_t get_32() { return fac.get_32(); }

	uint64_t get_64() { return fac.get_64(); }

	float get_float() { return fac.get_float(); }

	double get_double() { return fac.get_double(); }

	real_t get_real() { return fac.get_real(); }

	Vector<String> get_csv_line() { return fac.get_csv_line(); }

	PoolByteArray get_buffer(int len) {
		PoolByteArray pba;
		pba.resize(len);
		fac.get_buffer(pba.write().ptr(), len);
		return pba;
	}

	void flush() { fac.flush(); }

	String get_line() { return fac.get_line(); }

	void seek(int64_t position) { fac.seek(position); }

	void seek_end(int64_t position) {
		fac.seek_end(position);
	}

	void store_8(uint8_t value) { fac.store_8(value); }
	void store_16(uint16_t value) { fac.store_16(value); }
	void store_32(uint32_t value) { fac.store_32(value); }
	void store_64(uint64_t value) { fac.store_64(value); }

	void store_float(float value) { fac.store_float(value); }
	void store_double(double value) { fac.store_double(value); }
	void store_real(float value) { fac.store_real(value); }

	void store_buffer(PoolByteArray buffer) { fac.store_buffer(buffer.read().ptr(), buffer.size()); }
	void store_line(String line) { fac.store_line(line); }

	void store_csv_line(PoolStringArray values, String delim = ",") {
		Vector<String> v;
		v.resize(values.size());
		for (int i = 0; i < values.size(); i++) {
			v.push_back(values[i]);
		}
		fac.store_csv_line(v, delim);
	}

	void store_pascal_string(String string) { fac.store_pascal_string(string); }
	void store_string(String string) { fac.store_string(string); }

	void store_var(const Variant &p_var, bool p_full_objects) {
		int len;
		Error err = encode_variant(p_var, NULL, len, p_full_objects);
		ERR_FAIL_COND(err != OK);

		PoolVector<uint8_t> buff;
		buff.resize(len);
		PoolVector<uint8_t>::Write w = buff.write();

		err = encode_variant(p_var, &w[0], len, p_full_objects);
		ERR_FAIL_COND(err != OK);
		w = PoolVector<uint8_t>::Write();

		store_32(len);
		store_buffer(buff);
	}

	void close() {

		if (fac.is_open())
			fac.close();
	}

	void save_cache(NodePath p_node_path) {
		 fac.save_cache(p_node_path);
	}
};

#endif // FILE_ACCESS_CACHED_H
