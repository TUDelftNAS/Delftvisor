#pragma once

#include <string>
#include <vector>

#include "slice.hpp"

class hypervisor {
	std::vector<slice> slices;
public:
	void load_configuration( std::string filename );
};
