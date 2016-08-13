#pragma once

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

class Hypervisor;

class DiscoveredLink : public boost::enable_shared_from_this<DiscoveredLink> {
	boost::asio::deadline_timer liveness_timer;
	Hypervisor* hypervisor;
	int switch_id_1;
	uint32_t port_number_1;
	int switch_id_2;
	uint32_t port_number_2;

	/// The callback when this discovered link times out
	void timeout(const boost::system::error_code& error);

public:
	DiscoveredLink(
		boost::asio::io_service& io,
		Hypervisor* hypervisor,
		int switch_id_1,
		uint32_t port_number_1,
		int switch_id_2,
		uint32_t port_number_2);

	/// Return the other switch id of this link
	int get_other_switch_id(int switch_id) const;
	/// Return the port on the switch connected to this link
	int get_port_number(int switch_id) const;

	/// Start this discovered link
	void start();
	/// Stop this discovered link
	void stop();

	/// Reset the liveness timer
	void reset_timer();

	void print_to_stream(std::ostream& os) const;
};

std::ostream& operator<<(std::ostream& os, const DiscoveredLink& con);
