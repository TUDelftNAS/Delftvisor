#pragma once

#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "slice.hpp"

/// The top-level class
class Hypervisor {
private:
	boost::asio::signal_set signals;
	boost::asio::ip::tcp::acceptor switch_acceptor;

	std::vector<Slice> slices;

	/// A signal has been received
	void handle_signals(
		const boost::system::error_code& error,
		int signal_number
	);

public:
	/// Construct a new hypervisor object
	Hypervisor( boost::asio::io_service& io );

	/// Start running the hypervisor
	void start();
	/// Stop running the hypervisor
	void stop();

	/// Load configuration from file
	void load_configuration( std::string filename );
};
