#pragma once

#include <string>
#include <vector>
#include <list>
#include <unordered_map>

#include <boost/asio.hpp>

#include "slice.hpp"
#include "physical_switch.hpp"

/// The top-level class
class Hypervisor {
private:
	boost::asio::signal_set signals;
	boost::asio::ip::tcp::acceptor switch_acceptor;

	/// The slices in this hypervisor
	std::vector<Slice> slices;

	/// The physical switches registered at this hypervisor
	std::list<PhysicalSwitch::pointer> new_physical_switches;
	std::unordered_map<int64_t,PhysicalSwitch::pointer> identified_physical_switches;

	/// A signal has been received
	void handle_signals(
		const boost::system::error_code& error,
		int signal_number);

	/// Registers the handle_accept callback
	void start_accept();

	/// When a new switch connects
	void handle_accept(
		const boost::system::error_code& error,
		boost::shared_ptr<boost::asio::ip::tcp::socket> socket);

	/// Start listening for physical switch connections
	void start_listening( int port );

public:
	/// Construct a new hypervisor object
	Hypervisor( boost::asio::io_service& io );

	/// Lookup a physical switch by datapath_id
	PhysicalSwitch::pointer get_physical_switch_by_datapath_id(int64_t datapath_id);

	/// Start running the hypervisor
	void start();
	/// Stop running the hypervisor
	void stop();

	/// Load configuration from file
	void load_configuration( std::string filename );
};
