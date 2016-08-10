#include "discoveredlink.hpp"
#include "hypervisor.hpp"

#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>

DiscoveredLink::DiscoveredLink(
	boost::asio::io_service& io,
	Hypervisor* hypervisor,
	int switch_id_1,
	uint32_t port_number_1,
	int switch_id_2,
	uint32_t port_number_2)
:
	liveness_timer(io),
	hypervisor(hypervisor),
	switch_id_1(switch_id_1),
	port_number_1(port_number_1),
	switch_id_2(switch_id_2),
	port_number_2(port_number_2) {
}

void DiscoveredLink::timeout(const boost::system::error_code& error) {
	// If the code is operation_aborted it means that the timer
	// was reset or cancelled.
	if( error == boost::asio::error::operation_aborted ) return;

	BOOST_LOG_TRIVIAL(info) << "Link between " << switch_id_1 << " and " << switch_id_2 << " timed out";

	// TODO Delete link from physical switches
	hypervisor->get_physical_switch(switch_id_1)->reset_link(port_number_1);
	hypervisor->get_physical_switch(switch_id_2)->reset_link(port_number_2);

	hypervisor->calculate_routes();
}

int DiscoveredLink::get_other_switch_id(int switch_id) {
	if( switch_id == switch_id_1 ) {
		return switch_id_2;
	}
	else {
		return switch_id_1;
	}
}

int DiscoveredLink::get_port_number(int switch_id) {
	if( switch_id == switch_id_1 ) {
		return port_number_1;
	}
	else {
		return port_number_2;
	}
}

void DiscoveredLink::reset_timer() {
	// Reset the expiry date to further in the future
	liveness_timer.expires_from_now(
		boost::posix_time::milliseconds(1000));
	// When the expiration changes the handler is called
	// with error code operation_aborted
	liveness_timer.async_wait(
		boost::bind(
			&DiscoveredLink::timeout,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void DiscoveredLink::start() {
	reset_timer();
}

void DiscoveredLink::stop() {
	liveness_timer.cancel();
}
