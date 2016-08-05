#include "hypervisor.hpp"

#include <iostream>

#include <boost/bind.hpp>

Hypervisor::Hypervisor( boost::asio::io_service& io ) : signals(io, SIGINT, SIGTERM) {
}

void Hypervisor::handle_signals(
	const boost::system::error_code& error,
	int signal_number
) {
	if( !error ) {
		stop();
	}
}

void Hypervisor::start() {
	// Register the handler for signals
	signals.async_wait(boost::bind(
		&Hypervisor::handle_signals,
		this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::signal_number));
}

void Hypervisor::stop() {
	std::cout << "Stopping Hypervisor!" << std::endl;

	// Cancel the signal handler if it is still running
	signals.cancel();
}
