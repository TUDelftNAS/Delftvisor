#include "physical_switch.hpp"

PhysicalSwitch::pointer PhysicalSwitch::create(boost::asio::io_service& io) {
	return PhysicalSwitch::pointer( new PhysicalSwitch(io) );
}

PhysicalSwitch::PhysicalSwitch(boost::asio::io_service& io) :
	OpenflowConnection::OpenflowConnection(io),
	topology_discovery_timer(io) {
}

void PhysicalSwitch::handle_message() {
}

void PhysicalSwitch::start() {
	// Start up the generic connection handling
	OpenflowConnection::start();
}

void PhysicalSwitch::stop() {
	// Stop the generic connection handling
	OpenflowConnection::stop();
}

PhysicalSwitch::pointer PhysicalSwitch::shared_from_this() {
	return boost::static_pointer_cast<PhysicalSwitch>(OpenflowConnection::shared_from_this());
}
