#pragma once

#include <vector>

#include "virtual_switch.hpp"

class Slice {
	/// The internal id of this slice
	int id;
	/// The maximum rate this slice has
	int max_rate;

public:
	/// Construct a new slice
	Slice( int id, int max_rate );

	/// Start all the virtual switches in this slice
	void start();
	/// Stop all the virtual switches in this slice
	void stop();
};
