#pragma once

#include <vector>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "openflow_connection.hpp"

class VirtualSwitch;

struct PhysicalPort {
	int port_id;
	std::vector<VirtualSwitch*> dependent_virtual_switches;
};

class PhysicalSwitch : public OpenflowConnection {
private:
	/// The timer that when fired sends a topology discovery packet
	boost::asio::deadline_timer topology_discovery_timer;

	/// The constructor
	PhysicalSwitch(boost::asio::io_service& io);

	void schedule_topology_discovery_message();
	void send_topology_discovery_message();

protected:
	void handle_message();

public:
	typedef boost::shared_ptr<PhysicalSwitch> pointer;

	/// Allow creating a shared pointer of this class
	pointer shared_from_this();

	/// Create a new physical switch
	static pointer create(boost::asio::io_service& io);
	/// Start the switch, this means the socket is ready
	void start();
	/// Stop this switch
	void stop();
};
