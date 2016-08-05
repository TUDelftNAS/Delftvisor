#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "slice.hpp"

/// The top-level class
class Hypervisor {
private:
	boost::asio::signal_set signals;

	/// A signal has been received
	void handle_signals(
		const boost::system::error_code& error,
		int signal_number
	);

public:
	Hypervisor( boost::asio::io_service& io );
	/// Start running the hypervisor
	void start();
	/// Stop the hypervisor from running
	void stop();
};
