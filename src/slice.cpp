#include "slice.hpp"

#include <iostream>

Slice::Slice( int id, int rate ) : id(id), max_rate(max_rate) {
	std::cout << "Created slice with id=" << id << " rate=" << rate << std::endl;
}

void Slice::start() {
	std::cout << "Started slice " << id << std::endl;
}

void Slice::stop() {
	std::cout << "Stopped slice " << id << std::endl;
}
