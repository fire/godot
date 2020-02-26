/*************************************************************************/
/*  data_helpers.cpp                                                     */
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

#include "data_helpers.h"

#include "file_cache_manager.h"

DescriptorInfo::DescriptorInfo(FileAccess *fa, page_id new_range, int cache_policy) :
		offset(0), guid_prefix(new_range), cache_policy(cache_policy), valid(true) {
	ERR_FAIL_COND(!fa);
	internal_data_source = fa;
	switch (cache_policy) {
		case _FileCacheManager::KEEP:
			max_pages = CS_KEEP_THRESH_DEFAULT;
			break;
		case _FileCacheManager::LRU:
			max_pages = CS_LRU_THRESH_DEFAULT;
			break;
		case _FileCacheManager::FIFO:
			max_pages = CS_FIFO_THRESH_DEFAULT;
			break;
	}
	total_size = internal_data_source->get_len();
	path = internal_data_source->get_path();
	ready_sem = Semaphore::create();
	dirty_sem = Semaphore::create();
	lock = RWLock::create();
}

Variant DescriptorInfo::to_variant(FileCacheManager &p) {

	Dictionary d;


	for (int i = 0; i < pages.size(); ++i) {
		d[itoh(pages[i]) + " # " + itoh(p.page_frame_map[pages[i]])] = (p.frames->frames.write[p.page_frame_map[pages[i]]]->to_variant());
	}

	Dictionary out;
	out["offset"] = Variant(itoh(offset));
	out["total_size"] = Variant(itoh(total_size));
	out["guid_prefix"] = Variant(itoh(guid_prefix));
	out["pages"] = Variant(d);
	out["cache_policy"] = Variant(cache_policy);

	return Variant(out);
}
