#include "stt_queue.h"

String STTQueue::next() {
	String kw = keywords.read();
	return kw;
}

bool STTQueue::add(String kw) {
	keywords.write(kw);
	return true;
}

int STTQueue::size() {
	return keywords.size();
}

bool STTQueue::empty() {
	return !keywords.data_left();
}

void STTQueue::clear() {
	keywords.clear();
}

void STTQueue::set_capacity(const int &capacity) {
	real_t power = log2(capacity);
	power = Math::ceil(power);
	keywords.resize(power);
}

int STTQueue::get_capacity() {
	return keywords.size();
}

void STTQueue::_bind_methods() {
	ClassDB::bind_method("next",  &STTQueue::next);
	ClassDB::bind_method("size",  &STTQueue::size);
	ClassDB::bind_method("empty", &STTQueue::empty);
	ClassDB::bind_method("clear", &STTQueue::clear);

	ClassDB::bind_method(D_METHOD("set_capacity", "capacity"),
	                     &STTQueue::set_capacity);
	ClassDB::bind_method("get_capacity", &STTQueue::get_capacity);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "capacity", PROPERTY_HINT_DIR),
	               "set_capacity", "get_capacity");

	BIND_CONSTANT(DEFAULT_KWS_CAPACITY);
}

STTQueue::STTQueue() {
	set_capacity(DEFAULT_KWS_CAPACITY);
}

STTQueue::~STTQueue() {}
