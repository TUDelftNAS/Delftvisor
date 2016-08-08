#pragma once

#include <vector>
#include <string>

#include <boost/asio.hpp>

#include "virtual_switch.hpp"

class Slice {
private:
	/// The internal id of this slice
	int id;
	/// The maximum rate this slice has
	int max_rate;

	/// The controller endpoint of this slice
	boost::asio::ip::tcp::endpoint controller_endpoint;

	/// The virtual switches that exist in this slice
	std::vector<VirtualSwitch::pointer> virtual_switches;

	/// If this slice has been started
	bool started;

public:
	/// Construct a new slice
	Slice( int id, int max_rate, std::string ip_address, int port );

	/// Add a new virtual switch to this slice
	void add_new_virtual_switch(boost::asio::io_service& io, int64_t datapath_id);

	/// Get the endpoint this slice uses
	/**
	 * This is necessary for virtual switches to construct
	 * their socket.
	 */
	const boost::asio::ip::tcp::endpoint& get_controller_endpoint();

	/// Start allowing virtual switches in this slice to start
	void start();
	/// Stop all the virtual switches in this slice
	void stop();
	/// Return if this slice was started
	bool is_started();
};
