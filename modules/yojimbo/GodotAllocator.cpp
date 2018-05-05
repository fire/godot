#include "GodotAllocator.h"
#include "memory.h"
#include "thirdparty/yojimbo/shared.h"
#include "thirdparty/yojimbo/yojimbo.h"

GodotAllocator::GodotAllocator() {
	SetErrorLevel(ALLOCATOR_ERROR_NONE);
}

GodotAllocator::~GodotAllocator() {
	for (size_t i; i < memory.size(); i++) {
		Memory::free_static(memory[i]);
	}
}

void *GodotAllocator::Allocate(size_t size, const char *file, int line) {
	void *p = Memory::alloc_static(size);

	if (!p) {
		SetErrorLevel(ALLOCATOR_ERROR_OUT_OF_MEMORY);
		return NULL;
	}

	TrackAlloc(p, size, file, line);
	memory.push_back(p);
	return p;
}

void GodotAllocator::Free(void *p, const char *file, int line) {
	if (!p)
		return;

	TrackFree(p, file, line);

	memory.erase(p);
	Memory::free_static(p);
}
