/* Taskflow.cpp */

#include "taskflow.h"
#include "core/func_ref.h"
#include "core/os/os.h"
#include "thirdparty/cpp-taskflow/taskflow/taskflow.hpp"
#include <codecvt>
#include <locale>

Variant Taskflow::silent_emplace(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	const int func_ref_start = 3;
	ERR_FAIL_COND_V(p_argcount < func_ref_start, Variant());
	const int argcount = p_argcount - func_ref_start;
	for (size_t i = func_ref_start - 1; i < argcount; i++) {
		ERR_FAIL_COND_V(p_args[i]->get_type() != Variant::STRING, Variant())
	}
	const String name = *p_args[0];
	ERR_FAIL_COND_V(tasks.has(name), Variant());
	const Variant **argptrs = NULL;
	if (p_argcount != 0) {
		argptrs = (const Variant **)calloc(argcount, sizeof(Variant *));
		for (int i = 0; i < argcount; i++) {
			argptrs[i] = p_args[i + func_ref_start];
		}
	}
	Object *object = *p_args[1];
	String function = *p_args[2];
	auto task = taskflow.silent_emplace([object, function, argptrs, argcount, name]() {
		Variant::CallError ce = Variant::CallError();
		Ref<FuncRef> func;
		func.instance();
		func->set_instance(object);
		func->set_function(function);
		func->call_func(argptrs, argcount, ce);
		print_line(String("Task ") + name);
	});
	task.name(convert_string(name));
	tasks.insert(name, task);
	return Variant();
}

void Taskflow::preceeds(String src, String dest) {
	ERR_FAIL_COND(!tasks.has(src));
	tf::Task task = tasks.find(dest)->get();
	tasks.find(src)->get().precede(task);
}

Variant Taskflow::transform_reduce(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	// Check Types
	const int func_ref_start = 8;
	ERR_FAIL_COND_V(p_argcount < func_ref_start, Variant())
	const int argcount = p_argcount - func_ref_start;
	const String start_string = *p_args[0];
	const String target_string = *p_args[1];
	ERR_FAIL_COND_V(tasks.has(start_string), Variant());
	ERR_FAIL_COND_V(tasks.has(target_string), Variant());

	Vector<Variant> p_vec = *p_args[2];
	std::vector<Variant> vec;
	for (int i = 0; i < p_vec.size(); i++) {
		Variant element;
		element = p_vec[i];
		vec.push_back(element);
	}
	Object *reduce_object = *p_args[4];
	String reduce_function = *p_args[5];
	Object *transform_object = *p_args[6];
	String transform_function = *p_args[7];
	Variant result = *p_args[3];
	auto [start, target] = taskflow.transform_reduce(vec.begin(),
			vec.end(), result,
			[reduce_object, reduce_function](Variant l, Variant r) {
				Variant result;
				result = reduce_object->call(reduce_function, l, r);
				return result;
			},
			[transform_object, transform_function](Variant v) -> Variant {
				Variant result;
				result = transform_object->call(transform_function, v);
				return result;
			});
	start.name(convert_string(start_string));
	target.name(convert_string(target_string));
	tasks.insert(start_string, start);
	tasks.insert(target_string, target);
	auto future = taskflow.dispatch();
	future.get();
	tasks.clear();
	return result;
}

std::string Taskflow::convert_string(const String &source) {
	std::wstring string(source.c_str());
	using convert_type = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_type, wchar_t> converter;
	std::string converted_str = converter.to_bytes(string);
	return converted_str;
}

Variant Taskflow::broadcast(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	const int broadcast_start = 1;
	ERR_FAIL_COND_V(p_argcount < broadcast_start, Variant());
	// Check types
	for (size_t i = 0; i < p_argcount; i++) {
		ERR_FAIL_COND_V(p_args[i]->get_type() != Variant::STRING, Variant())
	}
	const int argcount = p_argcount - broadcast_start;
	String source = *p_args[0];
	ERR_FAIL_COND_V(!tasks.has(source), Variant());
	tf::Task task = tasks.find(source)->get();
	std::vector<tf::Task> vec;
	for (int i = 0 + broadcast_start; i < p_argcount; i++) {
		String node = *p_args[i];
		vec.push_back(tasks.find(node)->get());
	}
	task.broadcast(vec);
	return Variant();
}

void Taskflow::silent_dispatch() {
	taskflow.silent_dispatch();
	tasks.clear();
}
void Taskflow::dispatch() {
	result_future = taskflow.dispatch();
}

void Taskflow::wait_for_all() {
	taskflow.wait_for_all();
	tasks.clear();
}

void Taskflow::blocking_get() {
	result_future.get();
}

void Taskflow::dump_graph() {
	String out = taskflow.dump().c_str();
	print_line(out);
}

void Taskflow::_bind_methods() {
	{
		MethodInfo mi;
		mi.name = "silent_emplace";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "name"));
		mi.arguments.push_back(PropertyInfo(Variant::OBJECT, "instance"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "function"));
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "silent_emplace", &Taskflow::silent_emplace, mi);
	}
	{
		MethodInfo mi;
		mi.name = "broadcast";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "source"));
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "broadcast", &Taskflow::broadcast, mi);
	}
	{
		MethodInfo mi;
		mi.name = "transform_reduce";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "source"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "destination"));
		mi.arguments.push_back(PropertyInfo(Variant::ARRAY, "work"));
		mi.arguments.push_back(PropertyInfo(Variant::NIL, "default"));
		mi.arguments.push_back(PropertyInfo(Variant::OBJECT, "instance"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "transform"));
		mi.arguments.push_back(PropertyInfo(Variant::OBJECT, "instance"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "reduce"));
		ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "transform_reduce", &Taskflow::transform_reduce, mi);
	}
	ClassDB::bind_method(D_METHOD("preceeds", "source", "destination"), &Taskflow::preceeds);
	ClassDB::bind_method(D_METHOD("silent_dispatch"), &Taskflow::silent_dispatch);
	ClassDB::bind_method(D_METHOD("wait_for_all"), &Taskflow::wait_for_all);
	ClassDB::bind_method(D_METHOD("blocking_get"), &Taskflow::blocking_get);
	ClassDB::bind_method(D_METHOD("dispatch"), &Taskflow::dispatch);
	ClassDB::bind_method(D_METHOD("dump_graph"), &Taskflow::dump_graph);
}

Taskflow::Taskflow() :
		taskflow(OS::get_singleton()->get_processor_count()) {
}
