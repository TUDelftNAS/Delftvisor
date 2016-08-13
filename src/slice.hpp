#pragma once

#include <unordered_map>
#include <string>

#include <boost/asio.hpp>

#include "virtual_switch.hpp"

class Slice {
private:
	/// The internal id of this slice
	int id;
	/// The maximum rate this slice has
	int max_rate;

	/// The hypervisor this slice belongs to
	Hypervisor* hypervisor;

	/// The controller endpoint of this slice
	boost::asio::ip::tcp::endpoint controller_endpoint;

	/// The virtual switches that exist in this slice
	std::unordered_map<uint64_t,VirtualSwitch::pointer> virtual_switches;

	/// If this slice has been started
	bool started;

public:
	/// Construct a new slice
	Slice(
		int id,
		int max_rate,
		std::string ip_address,
		int port,
		Hypervisor* hypervisor);

	int get_id() const;
	int get_max_rate() const;

	/// Add a new virtual switch to this slice
	void add_new_virtual_switch(boost::asio::io_service& io, uint64_t datapath_id);
	/// Retreive a virtual switch
	VirtualSwitch::pointer get_virtual_switch_by_datapath_id(uint64_t datapath_id);

	/// Get the endpoint this slice uses
	/**
	 * This is used by virtual switches to construct
	 * their socket.
	 */
	const boost::asio::ip::tcp::endpoint& get_controller_endpoint();
	/// Get the virtual switches
	const std::unordered_map<uint64_t,VirtualSwitch::pointer>& get_virtual_switches() const;

	/// Start allowing virtual switches in this slice to start
	void start();
	/// Stop all the virtual switches in this slice
	void stop();
	/// Return if this slice was started
	bool is_started();

	/// For all the virtual switches in this slice check_online
	void check_online();
};
