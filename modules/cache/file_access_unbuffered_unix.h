/*************************************************************************/
/*  file_access_unbuffered_unix.h                                        */
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

#ifndef FILE_ACCESS_UNBUF_UNIX_H
#define FILE_ACCESS_UNBUF_UNIX_H

#include "core/os/file_access.h"
#include "core/os/memory.h"

#include "data_helpers.h"

#if defined(UNIX_ENABLED)

#include <sys/stat.h>
#include <unistd.h>

class FileAccessUnbufferedUnix : public FileAccess {

	enum CheckMode {
		CHK_MODE_SEEK,
		CHK_MODE_WRITE,
		CHK_MODE_READ
	};

	int fd;
	int pos;
	int flags;
	struct stat st;
	void check_errors() const;
	void check_errors(int val, int expected, int mode);
	mutable Error last_error;
	String save_path;
	String path;
	String path_src;

	static FileAccess *create_unbuf_unix();

public:

	Error unbuffered_open(const String &p_path, int p_mode_flags);
	Error _open(const String &p_path, int p_mode_flags); ///< open a file
	// Error open();
	void close(); ///< close a file
	bool is_open() const; ///< true when file is open

	String get_path() const; /// returns the path for the current open file
	String get_path_absolute() const; /// returns the absolute path for the current open file

	void seek(size_t p_position); ///< seek to a given position
	void seek_end(int64_t p_position = 0); ///< seek from the end of file
	size_t get_position() const; ///< get position in the file

	size_t get_len() const; ///< get size of the file

	bool eof_reached() const; ///< reading passed EOF

	uint8_t get_8() const; ///< get a byte
	int get_buffer(uint8_t *p_dst, int p_length) const;
	int get_buffer(uint8_t *p_dst, int p_length); // Use state info to catch errors.

	Error get_error() const; ///< get last error

	void flush();
	void store_8(uint8_t p_byte); ///< store a byte
	void store_buffer(const uint8_t *p_src, int p_length); ///< store an array of bytes

	bool file_exists(const String &p_path); ///< return true if a file exists

	uint64_t _get_modified_time(const String &p_file);

	Error _chmod(const String &p_path, int p_mod);

	FileAccessUnbufferedUnix();
	virtual ~FileAccessUnbufferedUnix();
};

#endif // if defined(UNIX_ENABLED)

#endif // FILE_ACCESS_UNBUF_UNIX_H
