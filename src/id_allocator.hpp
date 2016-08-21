#pragma once

#include <list>

/// Keep track and allocate id's
/**
 * A lot of virtual features need to be assigned
 * id's. This class keeps track of what id's are
 * used and what are still available.
 */
class IdAllocator {
	/// The highest possible allowed id
	int max;
	/// The next id to be released
	int next;

	/// The list of id's that have been returned
	std::list<int> returned_ids;

public:
	/// Create a new id allocator
	IdAllocator(int min, int max);
	/// Allocate a new id
	int new_id();
	/// Free a reserved id
	void free_id(int id);
	/// Return how many id's can still be allocated
	int amount_left();
};
