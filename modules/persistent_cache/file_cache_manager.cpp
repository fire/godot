/*************************************************************************/
/*  file_cache_manager.cpp                                               */
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

#include "file_cache_manager.h"
#include "file_access_cached.h"

#include "core/os/os.h"

#include <time.h>

#define RID_TO_DD(op) (uint64_t) rid op get_id() & 0x0000000000FFFFFF
#define RID_PTR_TO_DD RID_TO_DD(->)
#define RID_REF_TO_DD RID_TO_DD(.)

FileCacheManager::FileCacheManager() {
	mutex = Mutex::create();
	rng.set_seed(OS::get_singleton()->get_ticks_usec());

	memory_region = memnew_arr(uint8_t, CS_CACHE_SIZE);
	page_frame_map.clear();
	// pages.clear();
	frames.clear();

	available_space = CS_CACHE_SIZE;
	used_space = 0;
	total_space = CS_CACHE_SIZE;

	for (int i = 0; i < CS_NUM_FRAMES; ++i) {
		frames.push_back(
				memnew(FileCacheFrame(memory_region + i * CS_PAGE_SIZE)));
	}

	singleton = this;
}

FileCacheManager::~FileCacheManager() {
	//// WARN_PRINT("Destructor running.");
	if (memory_region) memdelete(memory_region);

	if (rids.size()) {
		for (const String *key = rids.next(NULL); key; key = rids.next(key)) {
			permanent_close(rids[*key]);
		}
	}

	if (files.size()) {
		const data_descriptor *key = NULL;
		for (key = files.next(NULL); key; key = files.next(key)) {
			memdelete(files[*key]);
		}
	}

	op_queue.sig_quit = true;
	op_queue.push(CtrlOp());
	exit_thread = true;

	Thread::wait_to_finish(this->thread);

	memdelete(thread);
	memdelete(mutex);
}

RID FileCacheManager::open(const String &path, int p_mode, int cache_policy) {

	//  WARN_PRINTS(path + " " + itoh(p_mode) + " " + itoh(cache_policy));

	ERR_FAIL_COND_V(path.empty(), RID());

	MutexLock ml = MutexLock(mutex);

	RID rid;

	if (rids.has(path)) {
		//  WARN_PRINTS("file already exists, reopening.");

		rid = rids[path];
		DescriptorInfo *desc_info = files[RID_REF_TO_DD];

		ERR_FAIL_COND_V_MSG(
				desc_info->valid,
				RID(),
				"The file " + path + " is already open.");

		CRASH_COND_MSG(desc_info->internal_data_source != NULL, "Descriptor in invalid state, internal data source is apparently valid!");

		desc_info->internal_data_source = FileAccess::open(desc_info->path, p_mode);

		// Seek to the previous offset.
		seek(rid, files[RID_REF_TO_DD]->offset);
		check_cache(rid, 8 * CS_PAGE_SIZE);
		desc_info->valid = true;

		if (desc_info->cache_policy != cache_policy) {
			for (int i = 0; i < desc_info->pages.size(); ++i) {
				CS_GET_CACHE_POLICY_FN(cache_removal_policies, desc_info->cache_policy)
				(desc_info->pages[i]);
				CS_GET_CACHE_POLICY_FN(cache_insertion_policies, cache_policy)
				(desc_info->pages[i]);
			}
			desc_info->cache_policy = cache_policy;
		}

	} else {
		// Will be freed when permanent_close is called with the corresponding RID.
		CachedResourceHandle *hdl = memnew(CachedResourceHandle);
		rid = handle_owner.make_rid(hdl);

		ERR_COND_MSG_ACTION(!rid.is_valid(), "Failed to create RID.", { memdelete(hdl); return RID(); });

		// PreferredFileAccessType *fa = memnew(FileAccessUnbufferedUnix);

		//Fail with a bad RID if we can't open the file.
		FileAccess *fa = NULL;
		ERR_COND_MSG_ACTION((fa = FileAccess::open(path, p_mode)) == NULL, "Could not open file.", { handle_owner.free(rid); memdelete(hdl); return RID(); });

		rids[path] = (add_data_source(rid, fa, cache_policy));
		//  WARN_PRINTS("open file " + path + " with mode " + itoh(p_mode) + "\nGot RID " + itoh(RID_REF_TO_DD) + "\n");
	}

	return rid;
}

void FileCacheManager::close(const RID rid) {

	DescriptorInfo *const *elem = files.getptr(RID_REF_TO_DD);
	ERR_FAIL_COND_MSG(!elem, String("No such file"))

	DescriptorInfo *desc_info = *elem;

	if (desc_info->internal_data_source)
		enqueue_flush_close(desc_info);
	else
		ERR_PRINTS("File already closed.");

	// This semaphore is triggered by do_flush_close_op.
	while (desc_info->valid == true)
		desc_info->ready_sem->wait();

	//  WARN_PRINTS("Closed file " + desc_info->path);
}

void FileCacheManager::permanent_close(const RID rid) {
	//  WARN_PRINTS("permanently closed file with RID " + itoh(RID_REF_TO_DD));
	MutexLock ml = MutexLock(mutex);
	close(rid);
	remove_data_source(rid);
	handle_owner.free(rid);
	memdelete(static_cast<CachedResourceHandle *>(rid.get_data()));
}

// Error FileCacheManager::reopen(const RID rid, int mode) {
// 	DescriptorInfo *di = files[RID_REF_TO_DD];
// 	if (!di->valid) {
// 		di->valid = true;
// 		return di->internal_data_source->reopen(di->path, mode);

// 	} else {
// 		return di->internal_data_source->reopen(di->path, mode);
// 		// FIXME: BAD SIDE EFFECTS OF CHANGING FILE MODES!!!
// 	}
// }

// Register a file handle with the cache manager.
// This function takes a pointer to a FileAccess object,
// so anything that implements the FileAccess API (from the file system, or from the network)
// can act as a data source.
RID FileCacheManager::add_data_source(RID rid, FileAccess *data_source, int cache_policy) {

	CRASH_COND(rid.is_valid() == false);
	data_descriptor dd = RID_REF_TO_DD;

	files[dd] = memnew(DescriptorInfo(data_source, (page_id)dd << 40, cache_policy));
	files[dd]->valid = true;

	CRASH_COND(files[dd] == NULL);

	seek(rid, 0, SEEK_SET);
	check_cache(rid, (cache_policy == _FileCacheManager::KEEP ? CS_KEEP_THRESH_DEFAULT : cache_policy == _FileCacheManager::LRU ? CS_LRU_THRESH_DEFAULT : CS_FIFO_THRESH_DEFAULT) * CS_PAGE_SIZE);

	return rid;
}

void FileCacheManager::remove_data_source(RID rid) {
	DescriptorInfo *di = files[RID_REF_TO_DD];

	for (int i = 0; i < di->pages.size(); i++) {

		frames.write[page_frame_map[di->pages[i]]]->wait_clean(di->dirty_sem)->set_ready_false()->set_used(false)->set_owning_page(0);

		memset(
				FileCacheFrame::DataWrite(
						frames.write[page_frame_map[di->pages[i]]],
						di,
						true)
						.ptr(),
				0,
				4096);
	}

	rids.erase(di->path);
	files.erase(di->guid_prefix >> 40);
	memdelete(di);
}

void FileCacheManager::enqueue_load(DescriptorInfo *desc_info, frame_id curr_frame, size_t offset) {
	//WARN_PRINTS("Enqueueing load for file " + desc_info->path + " at frame " + itoh(curr_frame) + " at offset " + itoh(offset))

	if (offset > desc_info->total_size) {

		// We can zero fill the current frame and return if the
		// current page is higher than the size of the file, to
		// prevent accidentally reading old data.

		//  WARN_PRINTS("Accessed out of bounds, reading zeroes.");
		memset(FileCacheFrame::DataWrite(frames.write[curr_frame], desc_info, true).ptr(), 0, CS_PAGE_SIZE);
		frames.write[curr_frame]->set_ready_true(desc_info->ready_sem);
		//  WARN_PRINTS("Finished OOB access.");
	} else {
		op_queue.push(CtrlOp(desc_info, curr_frame, offset, CtrlOp::LOAD));
		// WARN_PRINTS("file " + desc_info->path + " at offset " + itoh(offset) + " with frame " + itoh(curr_frame));
	}
}

void FileCacheManager::enqueue_store(DescriptorInfo *desc_info, frame_id curr_frame, size_t offset) {
	op_queue.push(CtrlOp(desc_info, curr_frame, offset, CtrlOp::STORE));
	//  WARN_PRINTS("Enqueue store op for file " + desc_info->path + " at offset " + itoh(offset) + " with frame " + itoh(curr_frame));
}

void FileCacheManager::enqueue_flush(DescriptorInfo *desc_info) {

	{
		MutexLock ml(op_queue.mut);
		for (List<CtrlOp>::Element *e = op_queue.queue.front(); e;) {
			List<CtrlOp>::Element *next = e->next();
			if (e->get().di == desc_info && e->get().type == CtrlOp::STORE) {
				//  WARN_PRINTS("Deleting store op with offset: " + itoh(e->get().offset) + " frame: " + itoh(e->get().frame) + " file:  " + e->get().di->path)

				e->erase();
			}
			e = next;
		}
	}

	op_queue.priority_push(CtrlOp(desc_info, CS_MEM_VAL_BAD, CS_MEM_VAL_BAD, CtrlOp::FLUSH));
	//  WARN_PRINTS("Enqueue flush op")
}

void FileCacheManager::enqueue_flush_close(DescriptorInfo *desc_info) {

	// WARN_PRINTS("Enqueue flush & close op")
	{
		MutexLock ml(op_queue.mut);
		for (List<CtrlOp>::Element *e = op_queue.queue.front(); e;) {
			List<CtrlOp>::Element *next = e->next();
			if (e->get().di == desc_info) {

				// Make it so the page frame mapping is removed as well.

				if (e->get().type == CtrlOp::LOAD) {
					DescriptorInfo *desc_info = e->get().di;
					untrack_page(desc_info, frames.write[e->get().frame]->owning_page);
				}

				e->erase();
			}
			e = next;
		}
	}
	op_queue.priority_push(CtrlOp(desc_info, CS_MEM_VAL_BAD, CS_MEM_VAL_BAD, CtrlOp::FLUSH_CLOSE));
}

void FileCacheManager::do_load_op(DescriptorInfo *desc_info, page_id curr_page, frame_id curr_frame, size_t offset) {
	// ERR_PRINTS("Start load op with file: " + desc_info->path + " page: " + itoh(curr_page) + " frame: " + itoh(curr_frame))

	CRASH_COND_MSG(desc_info->valid != true, "File not open!")

	desc_info->internal_data_source->seek(CS_GET_FILE_OFFSET_FROM_GUID(curr_page));
	int64_t used_size;
	{
		FileCacheFrame::DataWrite w(
				frames.write[curr_frame],
				desc_info,
				true);

		used_size = desc_info->internal_data_source->get_buffer(
				w.ptr(),
				CS_PAGE_SIZE);
		//ERR_PRINTS("File read returned " + itoh(used_size));

		// Error has occurred.
		CRASH_COND(used_size < 0);
		(frames.write[curr_frame])
				->set_used_size(used_size)
				->set_ready_true(desc_info->ready_sem);
	}
	// ERR_PRINTS(itoh(used_size) + " from offset " + itoh(offset) + " with page " + itoh(curr_page) + " mapped to frame " + itoh(curr_frame))
}

void FileCacheManager::do_store_op(DescriptorInfo *desc_info, page_id curr_page, frame_id curr_frame, size_t offset) {
	// store back to data source somehow...

	// ERR_PRINTS("Start store op with file: " + desc_info->path + " page: " + itoh(curr_page) + " frame: " + itoh(curr_frame))

	// Wait until the file is open.
	if (!desc_info->valid) {
		CRASH_NOW_MSG("File not open!") //(!desc_info->valid)
	}

	if (!desc_info->valid) {
		ERR_PRINTS("File not open!")
		CRASH_NOW() //(!desc_info->valid)
	}

	desc_info->internal_data_source->seek(CS_GET_PAGE(offset));
	{
		FileCacheFrame::DataRead r(frames.write[curr_frame], desc_info);

		desc_info->internal_data_source->store_buffer(r.ptr(), frames.write[curr_frame]->get_used_size());
		frames.write[curr_frame]->set_dirty_false(desc_info->dirty_sem, curr_frame);
	}

	// ERR_PRINTS("End store op with file: " + desc_info->path + " page: " + itoh(curr_page) + " frame: " + itoh(curr_frame))
}

void FileCacheManager::flush(RID rid) {
	enqueue_flush(files[RID_REF_TO_DD]);
}

void FileCacheManager::do_flush_op(DescriptorInfo *desc_info) {
	CRASH_COND(!(desc_info->internal_data_source));

	int j = 0;
	for (int i = 0; i < desc_info->pages.size(); i++) {
		if (frames.write[page_frame_map[desc_info->pages[i]]]->get_dirty()) {
			do_store_op(desc_info, page_frame_map[desc_info->pages[i]], desc_info->pages[i], page_frame_map[desc_info->pages[i]]);

			j += 1;
		}
	}
	// ERR_PRINTS("flushed file " + desc_info->path)
}

void FileCacheManager::do_flush_close_op(DescriptorInfo *desc_info) {
	CRASH_COND(!(desc_info->internal_data_source));

	for (int i = 0; i < desc_info->pages.size(); i++) {
		if (frames.write[page_frame_map[desc_info->pages[i]]]->get_dirty()) {
			do_store_op(desc_info, desc_info->pages[i], page_frame_map[desc_info->pages[i]], CS_GET_FILE_OFFSET_FROM_GUID(desc_info->pages[i]));
		}
	}

	desc_info->internal_data_source->close();
	memdelete(desc_info->internal_data_source);
	desc_info->internal_data_source = NULL;

	desc_info->dirty = false;
	desc_info->valid = false;
	// Posting on this semaphore allows FileCacheManager::close to continue executing.
	desc_info->ready_sem->post();

	// ERR_PRINTS("flushed and closed file " + desc_info->path)
}

// Perform a read operation.
size_t FileCacheManager::read(const RID rid, void *const buffer, size_t length) {

	DescriptorInfo **elem = files.getptr(RID_REF_TO_DD);

	ERR_FAIL_COND_V_MSG(!elem, CS_MEM_VAL_BAD, "No such file")

	DescriptorInfo *desc_info = *elem;
	size_t read_length = length;

	// If we try to read a region partially outside the file.
	{
		size_t end_offset = desc_info->offset + read_length;
		if (end_offset > desc_info->total_size) {
			//// WARN_PRINTS("Reached EOF before reading " + itoh(read_length) + " bytes.");
			read_length = desc_info->total_size - desc_info->offset;
		}
	}

	size_t initial_start_offset = desc_info->offset;
	size_t initial_end_offset = CS_GET_PAGE(initial_start_offset + CS_PAGE_SIZE);
	page_id curr_page;
	frame_id curr_frame;
	size_t buffer_offset = 0;

	// We need to handle the first and last frames differently,
	// because the data to be copied may not start at a page boundary, and may not end on a page boundary.
	{

		//  WARN_PRINTS("Getting page for offset " + itoh(desc_info->offset + buffer_offset) + " with start offset " + itoh(desc_info->offset))
		// Query for the page with the current offset.
		CRASH_COND((curr_page = get_page_guid(desc_info, desc_info->offset + buffer_offset, true)) == (page_id)CS_MEM_VAL_BAD);
		// Get frame mapped to page.
		CRASH_COND((curr_frame = page_frame_map[curr_page]) == (frame_id)CS_MEM_VAL_BAD);

		// The end offset of the first page may not be greater than the start offset of the next page.
		initial_end_offset = MIN(initial_start_offset + read_length, initial_end_offset);

		//  WARN_PRINTS("Reading first page with values:\ninitial_start_offset: " + itoh(initial_start_offset) + "\ninitial_end_offset: " + itoh(initial_end_offset) + "\n read size: " + itoh(initial_end_offset - initial_start_offset));

		{ // Lock the page holder for the operation.
			//  WARN_PRINTS("Locking frame " + itoh(curr_frame) + " with page " + itoh(curr_page))

			// wait before locking. Not after.
			frames.write[curr_frame]->wait_ready(desc_info->ready_sem);
			FileCacheFrame::DataRead r(frames.write[curr_frame], desc_info);

			// Here, frames.write[curr_frame].memory_region + CS_PARTIAL_SIZE(desc_info->offset)
			//  gives us the address of the first byte to copy which may or may not be on a page boundary.
			memcpy(
					(uint8_t *)buffer + buffer_offset,
					r.ptr() + CS_PARTIAL_SIZE(initial_start_offset),
					initial_end_offset - initial_start_offset);
		}

		buffer_offset += (initial_end_offset - initial_start_offset);
		read_length -= buffer_offset;
	}

	// Pages in the middle must be copied in full.
	while (buffer_offset < CS_GET_PAGE(length) && read_length > CS_PAGE_SIZE) {

		// Query for the page with the current offset.
		CRASH_COND((curr_page = get_page_guid(desc_info, desc_info->offset + buffer_offset, true)) == (page_id)CS_MEM_VAL_BAD);
		// Get frame mapped to page.
		CRASH_COND((curr_frame = page_frame_map[curr_page]) == (frame_id)CS_MEM_VAL_BAD);

		//  WARN_PRINTS("Reading intermediate page.\nbuffer_offset: " + itoh(buffer_offset) + "\nread_length: " + itoh(read_length) + "\ncurrent offset: " + itoh(desc_info->offset));

		// Lock current frame.
		{
			//  WARN_PRINTS("Locking frame " + itoh(curr_frame) + " with page " + itoh(curr_page))

			// wait before locking.
			frames.write[curr_frame]->wait_ready(desc_info->ready_sem);
			FileCacheFrame::DataRead r(frames.write[curr_frame], desc_info);

			memcpy(
					(uint8_t *)buffer + buffer_offset,
					r.ptr(),
					CS_PAGE_SIZE);
		}

		buffer_offset += CS_PAGE_SIZE;
		read_length -= CS_PAGE_SIZE;
	}

	// For final potentially partially filled page
	if (read_length) {

		// Query for the page with the current offset.
		CRASH_COND((curr_page = get_page_guid(desc_info, desc_info->offset + buffer_offset, true)) == (page_id)CS_MEM_VAL_BAD);
		// Get frame mapped to page.
		CRASH_COND((curr_frame = page_frame_map[curr_page]) == (frame_id)CS_MEM_VAL_BAD);

		// This is if
		size_t temp_read_len = MIN(read_length, frames.write[curr_frame]->get_used_size());

		//  WARN_PRINTS("Reading last page.\nread_length: " + itoh(read_length) + "\ntemp_read_len: " + itoh(temp_read_len));

		{ // Lock last page for reading data.
			//  WARN_PRINTS("Locking frame " + itoh(curr_frame) + " with page " + itoh(curr_page))

			// wait before locking.
			frames.write[curr_frame]->wait_ready(desc_info->ready_sem);
			FileCacheFrame::DataRead r(frames.write[curr_frame], desc_info);

			memcpy(
					(uint8_t *)buffer + buffer_offset,
					r.ptr(),
					temp_read_len);
		}
		buffer_offset += temp_read_len;
		read_length -= temp_read_len;
	}

	if (read_length > 0) // WARN_PRINTS("Unread length: " + itoh(length - read_length) + " bytes.")

		ERR_PRINTS("Read only " + itos(length - read_length) + " of " + itos(length) + "  bytes.\nFinal page: " + itoh(curr_page) + " Final frame: " + itoh(curr_frame));

	// TODO: Document this. Reads that exceed EOF will cause the remaining buffer space to be zeroed out.
	if ((desc_info->offset + length) / desc_info->total_size > 0) {
		memset((uint8_t *)buffer + (desc_info->total_size - desc_info->offset), '\0', length - read_length);
	}

	// We update the current offset at the end of the operation.
	desc_info->offset += buffer_offset;

	return buffer_offset;
}

// Similar to the read operation but opposite data flow.
size_t FileCacheManager::write(const RID rid, const void *const data, size_t length) {
	DescriptorInfo **elem = files.getptr(RID_REF_TO_DD);

	ERR_FAIL_COND_V_MSG(!elem, CS_MEM_VAL_BAD, "No such file")

	DescriptorInfo *desc_info = *elem;
	size_t write_length = length;

	size_t initial_start_offset = desc_info->offset;
	size_t initial_end_offset = CS_GET_PAGE(initial_start_offset + CS_PAGE_SIZE);
	page_id curr_page;
	frame_id curr_frame;
	size_t data_offset = 0;

	// We need to handle the first and last frames differently,
	// because the data to be copied may not start at a page boundary, and may not end on a page boundary.
	{

		//// WARN_PRINTS("Getting page for offset " + itoh(desc_info->offset + data_offset) + " with start offset " + itoh(desc_info->offset))
		// Query for the page with the current offset.
		CRASH_COND((curr_page = get_page_guid(desc_info, desc_info->offset + data_offset, true)) == (page_id)CS_MEM_VAL_BAD);
		// Get frame mapped to page.
		CRASH_COND((curr_frame = page_frame_map[curr_page]) == (frame_id)CS_MEM_VAL_BAD);

		// The end offset of the first page may not be greater than the start offset of the next page.
		initial_end_offset = MIN(initial_start_offset + write_length, initial_end_offset);
		//  WARN_PRINTS("Reading first page with values:\ninitial_start_offset: " + itoh(initial_start_offset) + "\ninitial_end_offset: " + itoh(initial_end_offset) + "\n read size: " + itoh(initial_end_offset - initial_start_offset));

		{ // Lock the page holder for the operation.

			// wait before locking. not after.
			frames.write[curr_frame]->wait_ready(desc_info->ready_sem);
			FileCacheFrame::DataWrite w(frames.write[curr_frame], desc_info, false);

			// Here, frames.write[curr_frame].memory_region + PARTIAL_SIZE(desc_info->offset)
			//  gives us the address of the first byte to copy which may or may not be on a page boundary.
			//
			// We can copy only CS_PAGE_SIZE - PARTIAL_SIZE(desc_info->offset) which gives us the number
			//  of bytes from the current offset to the end of the page.
			memcpy(
					w.ptr() + CS_PARTIAL_SIZE(initial_start_offset),
					(uint8_t *)data + data_offset,
					initial_end_offset - initial_start_offset);

			// If we're using less than a full page, we add our current
			// size to the used_size value.
			if (frames.write[curr_frame]->get_used_size() == CS_PAGE_SIZE) {
			} else if (
					CS_PARTIAL_SIZE(initial_end_offset) >
					frames.write[curr_frame]->get_used_size()) {
				frames.write[curr_frame]->set_used_size(CS_PARTIAL_SIZE(initial_end_offset));
			}
			frames.write[curr_frame]->set_dirty_true();
		}

		// If we've reached here, it means the cached file is dirty.
		desc_info->dirty = true;
		data_offset += (initial_end_offset - initial_start_offset);
		write_length -= data_offset;
	}

	// Pages in the middle must be copied in full.
	while (data_offset < CS_GET_PAGE(write_length) && write_length > CS_PAGE_SIZE) {

		// Query for the page with the current offset.
		CRASH_COND((curr_page = get_page_guid(desc_info, desc_info->offset + data_offset, true)) == (page_id)CS_MEM_VAL_BAD);
		// Get frame mapped to page.
		CRASH_COND((curr_frame = page_frame_map[curr_page]) == (frame_id)CS_MEM_VAL_BAD);

		// Here, frames.write[curr_frame].memory_region + PARTIAL_SIZE(desc_info->offset) gives us the start
		//  WARN_PRINTS("Writing intermediate page. data_offset: " + itoh(data_offset) + "\nwrite_length: " + itoh(write_length) + "\ncurrent offset: " + itoh(desc_info->offset));

		// Lock current page holder.
		{
			// wait before locking.
			frames.write[curr_frame]->wait_ready(desc_info->ready_sem);
			FileCacheFrame::DataWrite w(frames.write[curr_frame], desc_info, false);

			memcpy(
					w.ptr(),
					(uint8_t *)data + data_offset,
					CS_PAGE_SIZE);

			frames.write[curr_frame]->set_dirty_true();
		}

		data_offset += CS_PAGE_SIZE;
		write_length -= CS_PAGE_SIZE;
	}

	// For final potentially partially filled page
	if (write_length) {

		// Query for the page with the current offset.
		CRASH_COND((curr_page = get_page_guid(desc_info, desc_info->offset + data_offset, true)) == (page_id)CS_MEM_VAL_BAD);
		// Get frame mapped to page.
		CRASH_COND((curr_frame = page_frame_map[curr_page]) == (frame_id)CS_MEM_VAL_BAD);

		size_t temp_write_len = CLAMP(write_length, 0, frames.write[curr_frame]->get_used_size());
		//  WARN_PRINTS("Writing last page.\nwrite_length: " + itoh(write_length) + "\ntemp_write_len: " + itoh(temp_write_len));

		{ // Lock last page for reading data.

			// wait before locking.
			frames.write[curr_frame]->wait_ready(desc_info->ready_sem);
			FileCacheFrame::DataWrite w(frames.write[curr_frame], desc_info, false);

			memcpy(
					w.ptr(),
					(uint8_t *)data + data_offset,
					temp_write_len);

			// If we're using less than a full page, we add our current
			// size to the used_size value.
			if (frames.write[curr_frame]->get_used_size() == CS_PAGE_SIZE) {
			} else {
				if (CS_PARTIAL_SIZE(temp_write_len) > frames.write[curr_frame]->get_used_size())
					frames.write[curr_frame]->set_used_size(temp_write_len);
			}

			frames.write[curr_frame]->set_dirty_true();
		}
		data_offset += temp_write_len;
		write_length -= temp_write_len;
	}
	if (write_length > 0) ERR_PRINTS("Wrote only: " + itos(length - write_length) + " bytes.")

	desc_info->offset += data_offset;

	return data_offset;
}

// The seek operation just uses the POSIX seek modes.
size_t FileCacheManager::seek(const RID rid, int64_t new_offset, int mode) {

	DescriptorInfo **elem = files.getptr(RID_REF_TO_DD);

	ERR_FAIL_COND_V_MSG(!elem, CS_MEM_VAL_BAD, "No such file")

	DescriptorInfo *desc_info = *elem;
	size_t curr_offset = desc_info->offset;
	size_t end_offset = desc_info->total_size;
	int64_t eff_offset = 0;
	switch (mode) {
		case SEEK_SET:
			eff_offset += new_offset;
			break;
		case SEEK_CUR:
			eff_offset += curr_offset + new_offset;
			break;
		case SEEK_END:
			eff_offset += end_offset + new_offset;
			break;
		default:
			ERR_PRINT("Invalid mode parameter.")
			return CS_MEM_VAL_BAD;
	}

	if (eff_offset < 0) {
		ERR_PRINT("Invalid offset.")
		return CS_MEM_VAL_BAD;
	}

	/**
	 * When the user seeks far away from the current offset,
	 * and if the io queue currently has pending operations,
	 * we can clear the queue of operations that affect
	 * our current file (and only those operations).
	 *
	 * That way, we can read ahead at the
	 * new offset without any waits, and we only need to
	 * do an inexpensive page replacement operation instead
	 * of waiting for a load to occur.
	 *
	 * Maybe this behaviour could be toggled.
	 */
	{
		// Lock to prevent any other threads from changing the queue.
		MutexLock ml(op_queue.mut);
		if (!op_queue.queue.empty()) {

			//  WARN_PRINT("Acquired client side queue lock.");

			// Look for load ops with the same file that are farther than a threshold distance away from our effective offset and remove them.
			for (List<CtrlOp>::Element *i = op_queue.queue.front(); i;) {
				if (
						// If the operation is being performed on the same file...
						i->get().di->guid_prefix == desc_info->guid_prefix &&
						// And the type of operation is a load...
						i->get().type == CtrlOp::LOAD &&
						// And the distance between the pages in the vicinity of the new region and the current offset is large enough...
						ABSDIFF(
								eff_offset + (CS_FIFO_THRESH_DEFAULT * CS_PAGE_SIZE / 2),
								(int64_t)i->get().offset) > CS_FIFO_THRESH_DEFAULT) {

					CtrlOp l = i->get();

					// We can unmap the pages.
					//  WARN_PRINTS("Unmapping out of range page " + itoh(CS_GET_PAGE(l.offset)) + " and frame " + itoh(l.frame) + " for file with RID " + itoh(rid.get_id()));

					untrack_page(l.di, CS_GET_PAGE(l.offset));

					List<CtrlOp>::Element *next = i->next();
					// FIXME: Why is this causing an exception on windows?
					i->erase();
					i = next;

				} else
					i = i->next();
			}
		}
		//// WARN_PRINT("Released client side queue lock.");
	}

	// Update the offset.
	desc_info->offset = eff_offset;

	return eff_offset;
}

size_t FileCacheManager::get_len(const RID rid) const {

	DescriptorInfo *const *elem = files.getptr(RID_REF_TO_DD);

	ERR_FAIL_COND_V_MSG(!elem, CS_MEM_VAL_BAD, "No such file");

	DescriptorInfo *desc_info = *elem;

	size_t size = desc_info->internal_data_source->get_len();
	if (size > desc_info->total_size) {
		desc_info->total_size = size;
	}

	return size;
}

bool FileCacheManager::file_exists(const String &p_name) const {
	FileAccess *f = FileAccess::create(FileAccess::ACCESS_FILESYSTEM);
	bool exists = f->file_exists(p_name);
	memdelete(f);
	return exists;
}

bool FileCacheManager::eof_reached(const RID rid) const {

	DescriptorInfo *const *elem = files.getptr(RID_REF_TO_DD);

	ERR_FAIL_COND_V_MSG(!elem, true, "No such file");

	return (*elem)->internal_data_source->eof_reached();
}

void FileCacheManager::rmp_lru(page_id curr_page) {
	//  WARN_PRINTS("Removing LRU page " + itoh(curr_page));
	if (lru_cached_pages.find(curr_page) != NULL)
		lru_cached_pages.erase(curr_page);
}

void FileCacheManager::rmp_fifo(page_id curr_page) {
	//  WARN_PRINTS("Removing FIFO page " + itoh(curr_page));
	if (fifo_cached_pages.find(curr_page) != NULL)
		fifo_cached_pages.erase(curr_page);
}

void FileCacheManager::rmp_keep(page_id curr_page) {
	//  WARN_PRINTS("Removing permanent page " + itoh(curr_page));
	if (permanent_cached_pages.find(curr_page) != NULL)
		permanent_cached_pages.erase(curr_page);
}

void FileCacheManager::ip_lru(page_id curr_page) {
	//  WARN_PRINT("LRU cached.");
	lru_cached_pages.insert(curr_page);
}

void FileCacheManager::ip_fifo(page_id curr_page) {
	//  WARN_PRINT("FIFO cached.");
	fifo_cached_pages.push_front(curr_page);
}

void FileCacheManager::ip_keep(page_id curr_page) {
	//  WARN_PRINT("Permanent cached.");
	permanent_cached_pages.insert(curr_page);
}

void FileCacheManager::up_lru(page_id curr_page) {
	//  WARN_PRINTS("Updating LRU page " + itoh(curr_page));
	lru_cached_pages.erase(curr_page);
	frames.write[page_frame_map[curr_page]]->set_last_use(step)->set_ready_true(files[curr_page >> 40]->ready_sem);
	lru_cached_pages.insert(curr_page);
}
void FileCacheManager::up_fifo(page_id curr_page) {
	//  WARN_PRINTS("Updating FIFO page " + itoh(curr_page));
	frames.write[page_frame_map[curr_page]]->set_last_use(step)->set_ready_true(files[curr_page >> 40]->ready_sem);
}
void FileCacheManager::up_keep(page_id curr_page) {
	//  WARN_PRINTS("Updating Permanent page " + itoh(curr_page));
	permanent_cached_pages.erase(curr_page);
	frames.write[page_frame_map[curr_page]]->set_last_use(step)->set_ready_true(files[curr_page >> 40]->ready_sem);
	permanent_cached_pages.insert(curr_page);
}

/**
 * LRU replacement policy.
 */
page_id FileCacheManager::rp_lru(DescriptorInfo *desc_info) {

	page_id page_to_evict = CS_MEM_VAL_BAD;

	bool cond_flag = false;

	if (lru_cached_pages.size() > CS_LRU_THRESH_DEFAULT) {

		Ref<FileCacheFrame> f = frames.write[page_frame_map[lru_cached_pages.back()->get()]];

		if (step - f->get_last_use() > CS_LRU_THRESH_DEFAULT) {

			page_to_evict = (rng.randi() % 2) ? lru_cached_pages.back()->get() : lru_cached_pages.back()->prev()->get();

			lru_cached_pages.erase(page_to_evict);

		} else
			cond_flag = true;
	}

	if (cond_flag) {

		if (fifo_cached_pages.size() > CS_FIFO_THRESH_DEFAULT) {

			page_to_evict = fifo_cached_pages.back()->get();

			fifo_cached_pages.erase(fifo_cached_pages.back());

		} else if (lru_cached_pages.size() > 2) {

			page_to_evict = lru_cached_pages.back()->get();

			lru_cached_pages.erase(page_to_evict);

		} else {
			CRASH_NOW_MSG("CANNOT ADD LRU PAGE TO CACHE; INSUFFICIENT SPACE.")
		}
	}

	return page_to_evict;
}

page_id FileCacheManager::rp_keep(DescriptorInfo *desc_info) {

	page_id page_to_evict = CS_MEM_VAL_BAD;

	if (fifo_cached_pages.size() > CS_FIFO_THRESH_DEFAULT) {

		page_to_evict = fifo_cached_pages.back()->get();

		fifo_cached_pages.erase(fifo_cached_pages.back());

	} else if (lru_cached_pages.size() > CS_LRU_THRESH_DEFAULT) {

		Ref<FileCacheFrame> f = frames.write[page_frame_map[lru_cached_pages.back()->get()]];

		// The difference between the step and the last_use value of a frame gives us the frame's age.
		if (step - f->get_last_use() > CS_LRU_THRESH_DEFAULT) {

			page_to_evict = (rng.randi() % 2) ? lru_cached_pages.back()->get() : permanent_cached_pages.back()->prev()->get();

			lru_cached_pages.erase(page_to_evict);

		} else {
			page_to_evict = lru_cached_pages.back()->get();
			lru_cached_pages.erase(page_to_evict);
		}

	} else if (permanent_cached_pages.size() > CS_KEEP_THRESH_DEFAULT / 2) {

		page_to_evict = (rng.randi() % 2) ? permanent_cached_pages.back()->get() : permanent_cached_pages.back()->prev()->get();

		permanent_cached_pages.erase(page_to_evict);

	} else {
		CRASH_NOW_MSG("CANNOT ADD PERMANENT PAGE TO CACHE; INSUFFICIENT SPACE.")
	}

	return page_to_evict;
}

page_id FileCacheManager::rp_fifo(DescriptorInfo *desc_info) {

	page_id page_to_evict = CS_MEM_VAL_BAD;

	if (fifo_cached_pages.size() > CS_FIFO_THRESH_DEFAULT) {

		page_to_evict = fifo_cached_pages.back()->get();

		fifo_cached_pages.back()->erase();

	} else if (lru_cached_pages.size() > CS_LRU_THRESH_DEFAULT) {

		Ref<FileCacheFrame> f = frames.operator[](page_frame_map.operator[](lru_cached_pages.back()->get()));

		if (step - f->get_last_use() > CS_LRU_THRESH_DEFAULT) {

			page_to_evict = (rng.randi() % 2) ? lru_cached_pages.back()->get() : lru_cached_pages.back()->prev()->get();

			lru_cached_pages.erase(page_to_evict);
		}
	} else if (fifo_cached_pages.size() > CS_FIFO_THRESH_DEFAULT / 2) {

		page_to_evict = fifo_cached_pages.back()->get();

		fifo_cached_pages.back()->erase();

	} else {
		CRASH_NOW_MSG("CANNOT ADD FIFO PAGE TO CACHE; INSUFFICIENT SPACE.")
	}

	return page_to_evict;
}

bool FileCacheManager::get_page_or_do_paging_op(DescriptorInfo *desc_info, size_t offset) {

	page_id curr_page = get_page_guid(desc_info, offset, true);
	//  WARN_PRINTS("query for offset " + itoh(offset) + " : " + itoh(curr_page));
	frame_id curr_frame = CS_MEM_VAL_BAD;
	bool ret;

	if (curr_page == (page_id)CS_MEM_VAL_BAD) {

		curr_page = get_page_guid(desc_info, offset, false);
		//  WARN_PRINTS("Adding page : " + itoh(curr_page));

		// Find a free frame. last_used is only ever updated here, that could change...
		// TODO: change this to something more efficient.
		for (
				int i = ((last_used + 1) % CS_NUM_FRAMES);
				i != last_used;
				i = (i + 1) % 16) {

			if (frames.write[i]->get_used() == false) {

				// This is the only place where a frame's owning_page value is used, that could change.
				DescriptorInfo **old_desc_info = files.getptr(frames.write[i]->get_owning_page() >> 40);

				if (old_desc_info)
					frames.write[i]->wait_clean((*old_desc_info)->dirty_sem);

				frames.write[i]->set_ready_false()->set_used(true)->set_last_use(step)->set_used_size(0)->set_owning_page(curr_page);

				curr_frame = i;
				last_used = i;

				CRASH_COND(curr_frame == (frame_id)CS_MEM_VAL_BAD);
				CRASH_COND(page_frame_map.insert(curr_page, curr_frame) == NULL);

				//WARN_PRINTS(itoh(curr_page) + " mapped to " + itoh(curr_frame));
				CS_GET_CACHE_POLICY_FN(
						cache_insertion_policies,
						desc_info->cache_policy)
				(curr_page);
				break;
			}
		}

		// If there are no free frames, we evict an old one according to the paging/caching algo.
		if (curr_frame == (data_descriptor)CS_MEM_VAL_BAD) {
			//  WARN_PRINT("must evict");

			//  WARN_PRINTS("Cache policy: " + String(Dictionary(desc_info->to_variant(*this)).get("cache_policy", "-1")));

			// Call the appropriate replacement policy function for our caching policy.
			page_id page_to_evict = CS_GET_CACHE_POLICY_FN(cache_replacement_policies, desc_info->cache_policy)(desc_info);

			frame_id frame_to_evict = page_frame_map[page_to_evict];

			CRASH_COND(frame_to_evict == (frame_id)CS_MEM_VAL_BAD);

			if (frames.write[frame_to_evict]->get_dirty()) {
				enqueue_store(files[page_to_evict >> 40], frame_to_evict, CS_GET_FILE_OFFSET_FROM_GUID(page_to_evict));
			}

			untrack_page(files[page_to_evict >> 40], page_to_evict);

			// Set up flags and values for the new mapping.
			frames.write[frame_to_evict]->set_used(true)->set_last_use(step)->set_used_size(0)->set_owning_page(curr_page);

			// We reuse the page holder we evicted.
			curr_frame = frame_to_evict;

			//  WARN_PRINTS("evicted page under " + String(desc_info->cache_policy == _FileCacheManager::LRU ? "LRU " : (desc_info->cache_policy == _FileCacheManager::KEEP ? "KEEP " : "FIFO ")) + itoh(page_to_evict));

			CRASH_COND_MSG(page_frame_map.insert(curr_page, curr_frame) == NULL, "Could not insert new page in page-frame map.");

			CS_GET_CACHE_POLICY_FN(cache_insertion_policies, desc_info->cache_policy)
			(curr_page);

			//  WARN_PRINTS("curr_page : " + itoh(curr_page) + " mapped to curr_frame: " + itoh(curr_frame));
		}

		desc_info->pages.ordered_insert(curr_page);

		ret = false;

	} else {
		// Update cache related details...
		CS_GET_CACHE_POLICY_FN(cache_update_policies, desc_info->cache_policy)
		(curr_page);
		ret = true;
	}

	{
		MutexLock ml(mutex);
		step += 1;
	}

	return ret;
}

FileCacheManager *FileCacheManager::singleton = NULL;
_FileCacheManager *_FileCacheManager::singleton = NULL;

FileCacheManager *FileCacheManager::get_singleton() {
	return singleton;
}

void FileCacheManager::unlock() {
	if (!thread || !mutex) {
		return;
	}

	mutex->unlock();
}

void FileCacheManager::lock() {
	if (!thread || !mutex) {
		return;
	}

	mutex->lock();
}

Error FileCacheManager::init() {
	exit_thread = false;
	thread = Thread::create(FileCacheManager::thread_func, this);

	// th2 = Thread::create(FileCacheManager::th2_fn, this);

	return OK;
}

void FileCacheManager::thread_func(void *p_udata) {
	FileCacheManager &fcs = *static_cast<FileCacheManager *>(p_udata);

	do {

		// ERR_PRINTS("Thread" + itoh(fcs.thread->get_id()) + "Waiting for message.");
		CtrlOp l = fcs.op_queue.pop();
		//ERR_PRINT("got message");
		if (l.type == CtrlOp::QUIT)
			break;

		ERR_FAIL_COND_MSG(l.di == NULL, "Null file handle.")
		if(l.di->valid == false) {
			// ERR_PRINTS("Invalid file");
			fcs.untrack_page(l.di, get_page_guid(l.di, l.offset, false));
			continue;
		}

		page_id curr_page = get_page_guid(l.di, l.offset, false);
		frame_id curr_frame = fcs.page_frame_map[curr_page];

		switch (l.type) {
			case CtrlOp::LOAD: {
				// ERR_PRINTS("file: " + l.di->path + " Performing load for offset " + itoh(l.offset) + "\nIn pages: " + itoh(CS_GET_PAGE(l.offset)) + "\nCurr page: " + itoh(curr_page) + "\nCurr frame: " + itoh(curr_frame));
				fcs.do_load_op(l.di, curr_page, curr_frame, l.offset);
				break;
			}
			case CtrlOp::STORE: {
				// ERR_PRINTS("file: " + l.di->path + " Performing store.");
				fcs.do_store_op(l.di, curr_page, curr_frame, l.offset);
				break;
			}
			case CtrlOp::FLUSH: {
				// ERR_PRINTS("file: " + l.di->path + " Performing flush store.");
				fcs.do_flush_op(l.di);
				break;
			}
			case CtrlOp::FLUSH_CLOSE: {
				// ERR_PRINTS("file: " + l.di->path + " Performing flush store and close.")
				fcs.do_flush_close_op(l.di);
				break;
			}
			default: CRASH_NOW();
		}
	} while (!fcs.exit_thread);
}

void FileCacheManager::check_cache(const RID rid, size_t length) {

	DescriptorInfo *desc_info = files[RID_REF_TO_DD];

	if (length == CS_LEN_UNSPECIFIED) length = 8 * CS_PAGE_SIZE;

	for (page_id curr_page = CS_GET_PAGE(desc_info->offset); curr_page < CS_GET_PAGE(desc_info->offset + length) + CS_PAGE_SIZE; curr_page += CS_PAGE_SIZE) {
		//  WARN_PRINTS("Checking cache for file " + desc_info->path + " with offset " + itoh(curr_page));

		if (!get_page_or_do_paging_op(desc_info, curr_page)) {
			// TODO: reduce inconsistency here.
			//  WARN_PRINTS("get_page_or_do_paging_op result: curr_page: " + itoh(curr_page) + " curr_frame: " + itoh(page_frame_map[desc_info->guid_prefix | curr_page]))
			enqueue_load(desc_info, page_frame_map[desc_info->guid_prefix | curr_page], curr_page);
		}
	}
}

_FileCacheManager::_FileCacheManager() {
	singleton = this;
}

_FileCacheManager *_FileCacheManager::get_singleton() {
	return singleton;
}
