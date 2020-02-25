/*************************************************************************/
/*  control_queue.h                                                      */
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

#ifndef CTRL_QUEUE_H
#define CTRL_QUEUE_H

#include "core/list.h"
#include "core/os/mutex.h"
#include "core/os/semaphore.h"
#include "core/rid.h"

#include "data_helpers.h"

class CachedResourceHandle : public RID_Data {};

struct CtrlOp {
	enum Op {
		LOAD,
		STORE,
		QUIT,
		FLUSH,
		FLUSH_CLOSE,
	};

	DescriptorInfo *di;
	frame_id frame;
	size_t offset;
	uint8_t type;

	CtrlOp() :
			di(NULL),
			frame(CS_MEM_VAL_BAD),
			offset(CS_MEM_VAL_BAD),
			type(QUIT) {}

	CtrlOp(DescriptorInfo *i_di, frame_id frame, size_t i_offset, uint8_t i_type) :
			di(i_di),
			frame(frame),
			offset(i_offset),
			type(i_type) {}

	String as_string() const {
		return String("type: ") + (type == LOAD ? "LOAD" : type == STORE ? "STORE" : type == QUIT ? "QUIT" : type == FLUSH ? "FLUSH" : "FLUSH_CLOSE") +
			   "\noffset: " + itoh(offset) +
			   "\nframe: " + itoh(frame) +
			   "\nfile: " + (di ? di->path : "NULL") + "\n";
	}
};

class CtrlQueue {

	friend class FileCacheManager;

private:
	List<CtrlOp> queue;
	Mutex *mut;
	Semaphore *sem;

	CtrlOp pop() {
		// We keep this loop to catch the case where the semaphore is triggered even when the queue is empty.
		while (true) {

			while (queue.empty()) {
				sem->wait();
			}

			if (sig_quit) return CtrlOp();

			{ // We only need to lock when accessing the queue.
				MutexLock ml(mut);
				// If the queue isn't empty, we can pop. otherwise, loop back around and wait for the queue to be filled.
				if (!queue.empty()) {
					CtrlOp op = queue.front()->get();
					queue.pop_front();
					return op;
				}
			}
		}
	}

public:
	bool sig_quit;

	CtrlQueue() {
		mut = Mutex::create();
		sem = Semaphore::create();
		sig_quit = false;
	}

	~CtrlQueue() {
		memdelete(mut);
		memdelete(sem);
	}

	// Pushes to the back ofthe queue.
	void push(CtrlOp op) {
		MutexLock ml = MutexLock(mut);
		queue.push_back(op);
		// for (List<CtrlOp>::Element *i = queue.front(); i; i = i->next())
		// 	WARN_PRINTS("Curr op: " + String(i->get().type == CtrlOp::LOAD ? "load" : "other") + " op with page: " + itoh(i->get().offset) + " and frame: " + itoh(i->get().frame))
		sem->post();

		// WARN_PRINTS("Pushed " + String(op.type == CtrlOp::LOAD ? "load" : "other") + " op with page: " + itoh(op.offset) + " and frame: " + itoh(op.frame))
	}

	// Pushes to the queue's front, so the pushed operation is processed ASAP.
	void priority_push(CtrlOp op) {
		MutexLock ml = MutexLock(mut);
		queue.push_front(op);
		sem->post();
		// WARN_PRINTS("Priority pushed op.")
	}
};

#endif //CTRL_QUEUE_H
