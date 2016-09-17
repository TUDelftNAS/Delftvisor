#pragma once

#include <set>

/// Keep track and allocate id's
/**
 * A lot of virtual features need to be assigned
 * id's. This class keeps track of what id's are
 * used and what are still available.
 */
template<int min, int max>
class IdAllocator {
	/// The next id to be released
	int next;

	/// The set of id's that have been returned
	std::set<int> returned_ids;

public:
	/// Create a new id allocator
	IdAllocator() {
		next = min;
	}

	/// Allocate a new id
	int new_id() {
		int id;
		if( returned_ids.empty() ) {
			id = next;
			if( id > max ) {
				throw new std::out_of_range(
					"Cannot allocate new id, out of valid ids");
			}
			++next;
		}
		else {
			auto it = returned_ids.begin();
			id = *it;
			returned_ids.erase(it);
		}
		return id;
	}

	/// Free a reserved id
	void free_id(int id) {
		returned_ids.insert(id);

		// Clear id's from the returned id's
		std::set<int>::iterator it;
		while((it=returned_ids.find(next-1))!=returned_ids.end()) {
			returned_ids.erase(it);
			--next;
		}
	}

	/// Return how many id's can still be allocated
	int amount_left() {
		return returned_ids.size() + (max - next + 1);
	}
};
