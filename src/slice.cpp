#include "slice.hpp"

#include <iostream>
#include <string>

#include <boost/asio.hpp>

Slice::Slice( int id, int max_rate, std::string ip_address, int port ) :
	id(id),
	max_rate(max_rate),
	controller_endpoint(boost::asio::ip::address_v4::from_string(ip_address), port),
	started(false) {
}

void Slice::add_new_virtual_switch(boost::asio::io_service& io, int64_t datapath_id) {
	// Create the new virtual switch
	VirtualSwitch::pointer ptr( new VirtualSwitch(io, datapath_id, this) );

	// Check directly if this switch could go online, this only
	// makes sense if this slice has already been started but the
	// check_online function itself checks that.
	ptr->check_online();

	// Store the new switch in the list
	virtual_switches.emplace_back( ptr );
}

const boost::asio::ip::tcp::endpoint& Slice::get_controller_endpoint() {
	return controller_endpoint;
}

void Slice::start() {
	started = true;
	// Start all the virtual switches for which all physical
	// switches that are depended on are online. When physical
	// switches come online the virtual switches will poll
	// this slice to see if they can start.
	for( auto sw : virtual_switches ) {
		sw->check_online();
	}
}

void Slice::stop() {
	started = false;
	// Stop all virtual switches in this slice
	for( auto sw : virtual_switches ) {
		sw->stop();
	}
}

bool Slice::is_started() {
	return started;
}
