#include "thread_pool.h"

void ThreadPool::_bind_methods() {

	ClassDB::bind_method(D_METHOD("alloc"), &ThreadPool::alloc);
	ClassDB::bind_method(D_METHOD("start"), &ThreadPool::start);
	ClassDB::bind_method(D_METHOD("wait_to_finish"), &ThreadPool::wait_to_finish);
	ClassDB::bind_method(D_METHOD("get_thread_count"), &ThreadPool::get_thread_count);
}

void ThreadPool::alloc(size_t p_tc) {
	for (size_t i = 0; i < tc; i++) {
		memdelete(threads[i]);
	}
	threads.clear();
	for (size_t i = 0; i < p_tc; i++) {
		threads.push_back(memnew(_Thread));
	}
	tc = p_tc;
}

void ThreadPool::wait_to_finish(){
	for (size_t j = 0; j < tc; j++) {
		threads[j]->wait_to_finish();
	}
}

void ThreadPool::start(Object *p_instance, const StringName &p_method, const Variant &p_userdata, int p_priority) {
	for (size_t i = 0; i < tc; i++) {
		threads[i]->start(p_instance, p_method, p_userdata, p_priority);
	}
}

size_t ThreadPool::get_thread_count() const {
	return tc;
}

ThreadPool::ThreadPool() {
	tc = 0;
}

ThreadPool::~ThreadPool() {
	for (size_t i = 0; i < tc; i++) {
		memdelete(threads[i]);
	}
}
