#include "physical_switch.hpp"

#include <boost/log/trivial.hpp>

void PhysicalSwitch::schedule_topology_discovery_message() {
	// If there are no ports registered yet wait 1 period
	int wait_time;
	if( ports.size() == 0 ) {
		wait_time = topology::period;
	}
	else {
		wait_time = topology::period/ports.size();
	}

	// Schedule the topology message to be send
	topology_discovery_timer.expires_from_now(
		boost::posix_time::milliseconds(wait_time));
	topology_discovery_timer.async_wait(
		boost::bind(
			&PhysicalSwitch::send_topology_discovery_message,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void PhysicalSwitch::send_topology_discovery_message(
		const boost::system::error_code& error) {
	if( error.value() == boost::asio::error::operation_aborted ) {
		BOOST_LOG_TRIVIAL(trace) << *this
			<< " topology discovery timer cancelled";
		return;
	}
	else if( error ) {
		BOOST_LOG_TRIVIAL(error) << *this
			<< " topology discovery timer error: " << error.message();
		return;
	}

	// Figure out what port to send the packet over
	// Check if the port still exists!
	// TODO

	// Create the packet out message
	fluid_msg::of13::PacketOut packet_out;
	packet_out.xid(get_next_xid());
	// TODO Create the actual message and put it into the PacketOut

	// Send the message
	//send_message(packet_out);

	// Schedule the next message
	schedule_topology_discovery_message();
}

void PhysicalSwitch::reset_link(uint32_t port_number) {
	ports[port_number].link.reset();
	// This function is called when a discovered link times out.
	// Since both ends of that link has to be removed it doesn't
	// make sense to do the recalculation of the routes here.
}
