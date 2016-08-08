#pragma once

#include <vector>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "openflow_connection.hpp"

class PhysicalSwitch;
class Slice;

struct VirtualPort {
	PhysicalSwitch* physical_switch;
	int port_number;
};

class VirtualSwitch : public OpenflowConnection {
private:
	/// The datpath id of this switch
	int64_t datapath_id;

	/// The virtual ports on this switch
	std::vector<VirtualPort> ports;

	/// The slice this virtual switch belongs to
	Slice* slice;

	/// Try to connect to the controller
	void try_connect();
	/// The callback when the connection succeeds
	void handle_connect( const boost::system::error_code& error );
protected:
	/// Handle a received openflow message to this virtual switch
	void handle_message();
public:
	typedef boost::shared_ptr<VirtualSwitch> pointer;

	/// Allow creating a shared pointer of this class
	pointer shared_from_this();

	/// Create a new virtual switch object
	VirtualSwitch(boost::asio::io_service& io, int64_t datapath_id, Slice* slice);

	/// Check if this switch should be started/stopped
	/**
	 * This function is called after the topology of the physical
	 * network has changed. It checks if all physical switches this
	 * virtual switch depends on are online and if packets can be
	 * routed between all of them.
	 */
	void check_online();

	/// Start this virtual switch, try to connect to the controller
	void start();
	/// Stop the controller connection of this virtual switch
	void stop();
};
