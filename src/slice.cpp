#include "slice.hpp"

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/make_shared.hpp>

Slice::Slice(
		int id,
		int max_rate,
		std::string ip_address,
		int port,
		Hypervisor* hypervisor)
	:
		id(id),
		max_rate(max_rate),
		controller_endpoint(boost::asio::ip::address_v4::from_string(ip_address), port),
		hypervisor(hypervisor),
		started(false) {
}

int Slice::get_id() const {
	return id;
}

int Slice::get_max_rate() const {
	return max_rate;
}

void Slice::add_new_virtual_switch(
		boost::asio::io_service& io,
		uint64_t datapath_id) {
	// Create the new virtual switch
	VirtualSwitch::pointer ptr = boost::make_shared<VirtualSwitch>(
			io,
			datapath_id,
			hypervisor,
			this);

	// Check directly if this switch could go online, this only
	// makes sense if this slice has already been started but the
	// check_online function itself checks that.
	ptr->check_online();

	// Store the new switch in the list
	virtual_switches[datapath_id] = ptr;
}

VirtualSwitch::pointer Slice::get_virtual_switch_by_datapath_id(uint64_t datapath_id) {
	auto it = virtual_switches.find(datapath_id);
	if( it == virtual_switches.end() ) {
		return nullptr;
	}
	else {
		return it->second;
	}
}

const std::unordered_map<uint64_t,VirtualSwitch::pointer>& Slice::get_virtual_switches() const {
	return virtual_switches;
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
		sw.second->check_online();
	}
}

void Slice::stop() {
	started = false;

	// Stop all virtual switches in this slice
	for( auto& sw : virtual_switches ) {
		sw.second->go_down();
	}
}

bool Slice::is_started() {
	return started;
}

void Slice::check_online() {
	// If this slice hasn't started this doesn't make sense
	if( !started ) return;

	// Let all virtual switches check if they should go online
	for( auto& sw : virtual_switches ) {
		sw.second->check_online();
	}
}
