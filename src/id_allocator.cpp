#include "id_allocator.hpp"

#include <stdexcept>

IdAllocator::IdAllocator(int min, int max) :
		max(max),
		next(min) {
}

int IdAllocator::new_id() {
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
		id = returned_ids.back();
		returned_ids.pop_back();
	}

	return id;
}

void IdAllocator::free_id(int id) {
	returned_ids.push_front(id);
}

int IdAllocator::amount_left() {
	return returned_ids.size() + (max - next + 1);
}
