/* taskflow.h */
#ifndef TASKFLOW_H
#define TASKFLOW_H

#include "core/reference.h"
#include "thirdparty/cpp-taskflow/taskflow/taskflow.hpp"
#include <utility>

class Taskflow : public Reference {
	GDCLASS(Taskflow, Reference);

	tf::Taskflow taskflow;
	Map<String, tf::Task> tasks;
	std::shared_future<void> result_future;

protected:
	static void _bind_methods();

public:
	Variant silent_emplace(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	void preceeds(String source, String dest);
	Variant transform_reduce(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	std::string convert_string(const String &source);
	Variant broadcast(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	void silent_dispatch();
	void dispatch();
	void wait_for_all();
	void blocking_get();
	Taskflow();
};
#endif
