#include "file_access_unbuffered_unix.h"

#if defined(UNIX_ENABLED)

#include "core/os/os.h"
#include "core/print_string.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>

#ifndef ANDROID_ENABLED
#include <sys/statvfs.h>
#endif

void FileAccessUnbufferedUnix::check_errors() const {}

void FileAccessUnbufferedUnix::check_errors(int val, int expected, int mode) {

	CRASH_COND(fd < 0);

	switch (mode) {
		case CHK_MODE_SEEK:
			if (val >= st.st_size) {
				//ERR_PRINTS("Seeked to EOF.");
				last_error = ERR_FILE_EOF;
			} else if (val != expected) {
				ERR_PRINTS("Seeked to " + itoh(val) + " instead of " + itoh(expected));
				// CRASH_COND();
			}
		case CHK_MODE_WRITE:
			if (val == -1) {
				ERR_PRINTS("Write error with file " + this->path);
				last_error = ERR_FILE_CANT_WRITE;
			} else if (val != expected) {
				ERR_PRINTS("Wrote " + itoh(val) + " instead of " + itoh(expected) + " bytes from " + this->path);
				last_error = ERR_FILE_EOF;
			}
		case CHK_MODE_READ:
			if (val == -1) {
				ERR_PRINTS("Read error with file " + this->path);
				last_error = ERR_FILE_CANT_READ;
			} else if (val != expected) {
				ERR_PRINTS("Read " + itoh(val) + " instead of " + itoh(expected) + " bytes from " + this->path);
				last_error = ERR_FILE_EOF;
			}
	}
	return;
}

Error FileAccessUnbufferedUnix::_open(const String &p_path, int p_mode_flags) {

	path_src = p_path;
	path = fix_path(p_path);
	//printf("opening %ls, %i\n", path.c_str(), Memory::get_static_mem_usage());

	// If the fd is currently invalid, all good. We don't want to overwrite a valid fd by accident.
	ERR_FAIL_COND_V(fd != -1, ERR_ALREADY_IN_USE);
	int mode;

	if (p_mode_flags == READ)
		mode = O_RDONLY | O_SYNC | O_RSYNC | O_DSYNC;
	else if (p_mode_flags == WRITE)
		mode = O_WRONLY | O_TRUNC | O_CREAT | O_SYNC | O_RSYNC | O_DSYNC;
	else if (p_mode_flags == READ_WRITE)
		mode = O_RDWR | O_SYNC | O_RSYNC | O_DSYNC;
	else if (p_mode_flags == WRITE_READ)
		mode = O_RDWR | O_TRUNC | O_CREAT | O_SYNC | O_RSYNC | O_DSYNC;
	else
		return ERR_INVALID_PARAMETER;

	//printf("opening %s as %s\n", p_path.utf8().get_data(), path.utf8().get_data());
	int err = stat(path.utf8().get_data(), &st);
	if (!err) {
		switch (st.st_mode & S_IFMT) {
			case S_IFLNK:
			case S_IFREG:
				break;
			default:
				last_error = ERR_FILE_CANT_OPEN;
				return last_error;
		}
	}

	if (is_backup_save_enabled() && (p_mode_flags & WRITE) && !(p_mode_flags & READ)) {
		save_path = path;
		path = path + ".tmp";
	}

	fd = ::open(path.utf8().get_data(), mode);

	if (fd < 0) {
		last_error = ERR_FILE_CANT_OPEN;
		return last_error;
	} else {
		last_error = OK;
		flags = p_mode_flags;
		return last_error;
	}
}

Error FileAccessUnbufferedUnix::unbuffered_open(const String &p_path, int p_mode_flags) {
	return this->_open(p_path, p_mode_flags);
}

void FileAccessUnbufferedUnix::close() {

	if (fd < 0)
		return;

	::close(fd);
	fd = -1;

	if (close_notification_func) {
		close_notification_func(path, flags);
	}

	if (save_path != "") {
		int rename_error = rename((save_path + ".tmp").utf8().get_data(), save_path.utf8().get_data());

		if (rename_error && close_fail_notify) {
			close_fail_notify(save_path);
		}

		save_path = "";
		ERR_FAIL_COND(rename_error != 0);
	}
}

bool FileAccessUnbufferedUnix::is_open() const {

	return (fd > 0);
}

String FileAccessUnbufferedUnix::get_path() const {

	return path_src;
}

String FileAccessUnbufferedUnix::get_path_absolute() const {

	return path;
}

void FileAccessUnbufferedUnix::seek(size_t p_position) {

	ERR_FAIL_COND(fd < 0);

	last_error = OK;

	int old_pos = pos;

	if (p_position >= st.st_size) {

		pos = ::lseek(fd, 0, SEEK_END);
	} else {

		pos = ::lseek(fd, p_position, SEEK_SET);
	}

	ERR_COND_ACTION(pos < 0,);
	if (pos == -1) {
		pos = old_pos;
	}

	check_errors(pos, p_position, CHK_MODE_SEEK);
}

void FileAccessUnbufferedUnix::seek_end(int64_t p_position) {

	CRASH_COND(fd < 0);
	ERR_FAIL_COND(p_position > 0);
	ERR_FAIL_COND((p_position + st.st_size) < 0);

	last_error = OK;

	int new_pos = ::lseek(fd, p_position, SEEK_END);

	ERR_FAIL_COND(new_pos == -1);

	check_errors(new_pos, st.st_size - p_position, CHK_MODE_SEEK);

	pos = new_pos;
}

size_t FileAccessUnbufferedUnix::get_position() const {

	CRASH_COND(fd < 0);

	long pos = ::lseek(fd, 0, SEEK_CUR);
	ERR_FAIL_COND_V_MSG(pos < 0, -1, "lseek returned " + itos(pos));
	return pos;
}

size_t FileAccessUnbufferedUnix::get_len() const {

	CRASH_COND(fd < 0);

	// long pos = ::lseek(fd, 0, SEEK_CUR);
	// ERR_FAIL_COND_V(pos < 0, 0);
	// ERR_FAIL_COND_V(::lseek(fd, 0, SEEK_END), 0);
	struct stat st;
	CRASH_COND(fstat(fd, &st) < 0);
	// ERR_FAIL_COND_V(fseek(f, pos, SEEK_SET), 0);

	return st.st_size;
}

bool FileAccessUnbufferedUnix::eof_reached() const {

	return last_error == ERR_FILE_EOF;
}

uint8_t FileAccessUnbufferedUnix::get_8() const {

	CRASH_COND(fd < 0);
	uint8_t b;
	ERR_COND_MSG_ACTION(read(fd, &b, 1) < 1, "Could not read byte.", { b = '\0'; });
	return b;
}

// Implemented using direct unix read syscall.
int FileAccessUnbufferedUnix::get_buffer(uint8_t *p_dst, int p_length) const {

	CRASH_COND(fd < 0);
	return read(fd, p_dst, p_length);
};

Error FileAccessUnbufferedUnix::get_error() const {

	return last_error;
}

void FileAccessUnbufferedUnix::store_8(uint8_t p_byte) {

	CRASH_COND(fd < 0);
	ERR_FAIL_COND(write(fd, &p_byte, 1) != 1);
}

void FileAccessUnbufferedUnix::store_buffer(const uint8_t *p_src, int p_length) {
	CRASH_COND(fd < 0);
	ERR_FAIL_COND(write(fd, p_src, p_length) < p_length);
}

bool FileAccessUnbufferedUnix::file_exists(const String &p_path) {

	int err;
	String filename = fix_path(p_path);

	// Does the name exist at all?
	err = stat(filename.utf8().get_data(), &st);
	if (err)
		return false;

	// See if we have access to the file
	if (access(filename.utf8().get_data(), F_OK))
		return false;

	// See if this is a regular file
	switch (st.st_mode & S_IFMT) {
		case S_IFLNK:
		case S_IFREG:
			return true;
		default:
			return false;
	}
}

uint64_t FileAccessUnbufferedUnix::_get_modified_time(const String &p_file) {

	String file = fix_path(p_file);
	int err = stat(file.utf8().get_data(), &st);

	if (!err) {
		return st.st_mtime;
	} else {
		ERR_FAIL_V_MSG(-1, "Failed to get modified time for: " + p_file);
	};
}

Error FileAccessUnbufferedUnix::_chmod(const String &p_path, int p_mod) {
	int err = chmod(p_path.utf8().get_data(), p_mod);
	if (!err) {
		return OK;
	}

	return FAILED;
}

// Flush does not make sense for unbuffered IO so it has only checks and does not actually do anything.
void FileAccessUnbufferedUnix::flush() {

	ERR_FAIL_COND(fd < 0);
}

FileAccess *FileAccessUnbufferedUnix::create_unbuf_unix() {

	return memnew(FileAccessUnbufferedUnix);
}

CloseNotificationFunc FileAccessUnbufferedUnix::close_notification_func = NULL;

FileAccessUnbufferedUnix::FileAccessUnbufferedUnix() :
		fd(-1),
		flags(0),
		pos(0),
		last_error(OK) {
}

FileAccessUnbufferedUnix::~FileAccessUnbufferedUnix() {

	close();
}

#endif