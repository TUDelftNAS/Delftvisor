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

	/// The next switch id for a physical switch
	int next_physical_switch_id;
	/// The physical switches registered at this hypervisor
	std::unordered_map<int,PhysicalSwitch::pointer> physical_switches;
	/// A map from datapath id to switch id
	std::unordered_map<uint64_t,int> datapath_id_to_switch_id;

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

	/// Start running the hypervisor
	void start();
	/// Stop running the hypervisor
	void stop();

	/// Lookup a physical switch by switch id
	PhysicalSwitch::pointer get_physical_switch(int switch_id) const;
	/// Lookup a physical switch by datapath_id
	PhysicalSwitch::pointer get_physical_switch_by_datapath_id(uint64_t datapath_id) const;

	/// Get the physical switches in the hypervisor
	const std::unordered_map<int,PhysicalSwitch::pointer>& get_physical_switches() const;
	/// Get slices
	const std::vector<Slice>& get_slices() const;

	/// Register a physical switch
	void register_physical_switch(uint64_t datapath_id,int switch_id);
	/// Unregister a physical switch
	void unregister_physical_switch(int switch_id);
	void unregister_physical_switch(uint64_t datapath_id,int switch_id);

	/// Run the floyd-warshall algorithm on the known topology in dot format
	void calculate_routes();
	/// Print the found topology to an ostream
	void print_topology(std::ostream& os);

	/// Load configuration from file
	void load_configuration( std::string filename );
};
