#pragma once
#include "thirdparty/yojimbo/yojimbo.h"
#include "list.h"

class GodotAllocator : public yojimbo::Allocator {
public:
	/**
	Godot allocator constructor.
	If you want to integrate your own allocator with yojimbo for use with the client and server, this class is a good template to start from.
	Make sure your constructor has the same signature as this one, and it will work with the YOJIMBO_SERVER_ALLOCATOR and YOJIMBO_CLIENT_ALLOCATOR helper macros.
	@param memory Block of memory in which the allocator will work. This block must remain valid while this allocator exists. The allocator does not assume ownership of it, you must free it elsewhere, if necessary.
	@param bytes The size of the block of memory (bytes). The maximum amount of memory you can allocate will be less, due to allocator overhead.
	*/

	GodotAllocator();

	/**
	Godot allocator destructor.
	Checks for memory leaks in debug build. Free all memory allocated by this allocator before destroying.
	*/

	~GodotAllocator();

	/**
	Allocates a block of memory using TLSF.
	IMPORTANT: Don't call this directly. Use the YOJIMBO_NEW or YOJIMBO_ALLOCATE macros instead, because they automatically pass in the source filename and line number for you.
	@param size The size of the block of memory to allocate (bytes).
	@param file The source code filename that is performing the allocation. Used for tracking allocations and reporting on memory leaks.
	@param line The line number in the source code file that is performing the allocation.
	@returns A block of memory of the requested size, or NULL if the allocation could not be performed. If NULL is returned, the error level is set to ALLOCATION_ERROR_FAILED_TO_ALLOCATE.
	*/

	void *Allocate(size_t size, const char *file, int line) override;

	/**
	Free a block of memory using TLSF.
	IMPORTANT: Don't call this directly. Use the YOJIMBO_DELETE or YOJIMBO_FREE macros instead, because they automatically pass in the source filename and line number for you.
	@param p Pointer to the block of memory to free. Must be non-NULL block of memory that was allocated with this allocator. Will assert otherwise.
	@param file The source code filename that is performing the free. Used for tracking allocations and reporting on memory leaks.
	@param line The line number in the source code file that is performing the free.
	@see Allocator::Allocate
	@see Allocator::GetError
	*/

	void Free(void *p, const char *file, int line) override;

private:
	List<void *> memory;

	GodotAllocator(const GodotAllocator &other);
	GodotAllocator &operator=(const GodotAllocator &other);
};
