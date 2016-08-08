#include <boost/bind.hpp>

#include "slice.hpp"
#include "virtual_switch.hpp"
#include "physical_switch.hpp"

VirtualSwitch::VirtualSwitch(boost::asio::io_service& io, int64_t datapath_id, Slice *slice) :
	OpenflowConnection::OpenflowConnection(io),
	datapath_id(datapath_id),
	slice(slice) {
}

void VirtualSwitch::handle_message() {
}

void VirtualSwitch::try_connect() {
	socket.async_connect(
		slice->get_controller_endpoint(),
		boost::bind(
			&VirtualSwitch::handle_connect,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void VirtualSwitch::handle_connect( const boost::system::error_code& error ) {
	if( !error ) {
		OpenflowConnection::start();

		// Send PortStatus messages for each port
	}
	else {
		// Try connecting again?
		try_connect();
	}
}

void VirtualSwitch::check_online() {
	// If the slice hasn't been started don't do anything
	if( !slice->is_started() ) return;

	bool all_online_and_reachable = true;
	PhysicalSwitch* first_switch = nullptr;

	for( VirtualPort port : ports ) {
		// Lookup the PhysicalSwitch that owns this port
		// and make sure that switch is online

		if( first_switch == nullptr ) {
			// Set the current PhysicalSwitch in the first_switch
			// variable
		}
		else {
			// Check connectivity between the current PhysicalSwitch
			// and *first_switch
		}
	}

	// Update this virtual switch state if needed
	if( all_online_and_reachable && !socket.is_open() ) {
		start();
	}
	else if(!all_online_and_reachable && socket.is_open() ) {
		stop();
	}
}

void VirtualSwitch::start() {
	if( !socket.is_open() ) {
		try_connect();
	}
}

void VirtualSwitch::stop() {
	if( socket.is_open() ) {
		OpenflowConnection::stop();
		// socket.cancel()
		socket.close();
	}
}

VirtualSwitch::pointer VirtualSwitch::shared_from_this() {
	return boost::static_pointer_cast<VirtualSwitch>(OpenflowConnection::shared_from_this());
}
