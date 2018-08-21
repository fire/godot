#pragma once

#ifndef THEAD_POOL_H
#define THEAD_POOL_H

#include "reference.h"
#include "core/bind/core_bind.h"

//var threads : Array = []
//var tc
//
//func thread_pool_alloc(p_tc):
//	for i in range(p_tc):
//		threads.append(Thread.new())
//	tc = p_tc
//func thread_pool_wait_to_finish(a, instance, method, userdata, priority):
//	for i in range(tc):
//		threads[i].start(instance, method, userdata, priority)
//	for i in range(tc):
//		threads[i].wait_to_finish()

class ThreadPool : public Reference {
	GDCLASS(ThreadPool, Reference);

	Vector<_Thread *> threads;
	size_t tc;

protected:
	static void _bind_methods();

public:
	void alloc(size_t tc);
	void wait_to_finish();
	void start(Object *p_instance, const StringName &p_method, const Variant &p_userdata, int p_priority);
	size_t get_thread_count() const;

	ThreadPool();
	~ThreadPool();
};
#endif

