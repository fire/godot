/*************************************************************************/
/*  threaded_array_processor.h                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifndef THREADED_ARRAY_PROCESSOR_H
#define THREADED_ARRAY_PROCESSOR_H

#include "core/os/memory.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/thread_safe.h"
#include "core/safe_refcount.h"
#include "thirdparty/fem_fx_async/FEMFXAsyncThreading.h"
#include "thirdparty/fem_fx_async/FEMFXCommon.h"
#include "thirdparty/fem_fx_async/FEMFXParallelFor.h"
#include "thirdparty/fem_fx_async/FEMFXTaskSystemInterface.h"

using namespace AMD;

template <class C, class U>
struct ThreadArrayProcessData {
	uint32_t elements;
	C *instance;
	U userdata;
	void (C::*method)(uint32_t, U);
	void process(uint32_t p_index) {
		(instance->*method)(p_index, userdata);
	}
};

template <class C, class U>
void FmTaskProcessArray(void* inTaskData, int32_t inTaskBeginIndex, int32_t inTaskEndIndex) { 
	ThreadArrayProcessData<C, U> *taskData = (ThreadArrayProcessData<C, U> *) inTaskData;
	uint startIdx, endIdx;
	FmGetIndexRange(&startIdx, &endIdx, (uint)inTaskBeginIndex, OS::get_singleton()->get_processor_count(), taskData->elements);
	for (uint i = startIdx; i < endIdx; i++) {
		taskData->process(i);
	}
} 

template <class C, class M, class U>
void thread_process_array(uint32_t p_elements, C *p_instance, M p_method, U p_userdata) {

	ThreadArrayProcessData<C, U> data;
	data.method = p_method;
	data.instance = p_instance;
	data.userdata = p_userdata;
	data.elements = p_elements;
	int32_t num_threads = OS::get_singleton()->get_processor_count();
#ifndef NO_THREADS
	num_threads = 1;
#endif
	AMD::FmTaskSystemCallbacks callbacks;
	AMD::FmParallelForAsync("thread_process_array", FmTaskProcessArray<C, U>, FmTaskProcessArray<C, U>, NULL, &data, p_elements, callbacks.SubmitAsyncTask, num_threads);
}

#endif // THREADED_ARRAY_PROCESSOR_H
