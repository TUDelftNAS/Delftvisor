#include "hypervisor.hpp"
#include "discoveredlink.hpp"
#include "slice.hpp"
#include "physical_switch.hpp"
#include "vlan_tag.hpp"

#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include <boost/make_shared.hpp>

Hypervisor::Hypervisor( boost::asio::io_service& io ) :
	signals(io, SIGINT, SIGTERM),
	switch_acceptor(io),
	physical_switch_id_allocator(0,vlan_tag::max_switch_id) {
}

void Hypervisor::handle_signals(
	const boost::system::error_code& error,
	int signal_number
) {
	if( !error ) {
		BOOST_LOG_TRIVIAL(info) << "Received signal " << signal_number;
		stop();
	}
	else {
		BOOST_LOG_TRIVIAL(error) << "Error while receiving signal: " << error.message();
	}
}

void Hypervisor::start_accept() {
	boost::shared_ptr<boost::asio::ip::tcp::socket> new_socket =
		boost::make_shared<boost::asio::ip::tcp::socket>(
			switch_acceptor.get_io_service());

	switch_acceptor.async_accept(
		*new_socket,
		boost::bind(
			&Hypervisor::handle_accept,
			this,
			boost::asio::placeholders::error,
			new_socket));
}

void Hypervisor::handle_accept(
		const boost::system::error_code& error,
		boost::shared_ptr<boost::asio::ip::tcp::socket> socket) {
	if( !error ) {
		// Reserve a switch id
		int id = physical_switch_id_allocator.new_id();

		// Add the physical switch to the list
		physical_switches.emplace(
			id,
			boost::make_shared<PhysicalSwitch>(
				*socket,
				id,
				this));

		// And start the physical switch
		physical_switches.at(id)->start();

		// Start waiting for the next connection
		start_accept();
	}
	else if(error != boost::asio::error::operation_aborted) {
		BOOST_LOG_TRIVIAL(error) <<
			"Something went wrong while accepting a connection: " << error.message();
	}
}

void Hypervisor::register_physical_switch(uint64_t datapath_id, int switch_id) {
	datapath_id_to_switch_id[datapath_id] = switch_id;
}

void Hypervisor::unregister_physical_switch(int switch_id) {
	physical_switches.erase(switch_id);
	physical_switch_id_allocator.free_id(switch_id);
}
void Hypervisor::unregister_physical_switch(uint64_t datapath_id, int switch_id) {
	datapath_id_to_switch_id.erase(datapath_id);
	unregister_physical_switch(switch_id);
}

PhysicalSwitch::pointer Hypervisor::get_physical_switch(int switch_id) const {
	auto it = physical_switches.find(switch_id);
	if( it == physical_switches.end() ) {
		return nullptr;
	}
	else {
		return it->second;
	}
}
PhysicalSwitch::pointer Hypervisor::get_physical_switch_by_datapath_id(
		uint64_t datapath_id) const {
	auto it = datapath_id_to_switch_id.find(datapath_id);
	if( it == datapath_id_to_switch_id.end() ) {
		return nullptr;
	}
	else {
		return get_physical_switch(it->second);
	}
}
/// Get the physical switches in the hypervisor
const std::unordered_map<int,PhysicalSwitch::pointer>& Hypervisor::get_physical_switches() const {
	return physical_switches;
}
/// Get slices
const std::vector<Slice>& Hypervisor::get_slices() const {
	return slices;
}

void Hypervisor::start() {
	// Register the handler for signals
	signals.async_wait(boost::bind(
		&Hypervisor::handle_signals,
		this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::signal_number));

	// Register the acceptor for switch connections
	start_accept();

	// Start all of the slices
	for( Slice& s : slices ) s.start();
}

void Hypervisor::stop() {
	BOOST_LOG_TRIVIAL(trace) << "Stopping Hypervisor";

	// Cancel the signal handler if it is still running
	signals.cancel();

	// Stop accepting new switch connections, this also
	// cancels all pending operations on the acceptor
	switch_acceptor.close();

	// Stop all physical switches, deleting an entry in
	// an unordered_map causes all iterators to that
	// entry to be invalidated, which is why the iterator
	// is copied and incremented before calling stop.
	auto it = physical_switches.begin();
	while( it != physical_switches.end() ) {
		auto it_tmp = it;
		++it;
		it_tmp->second->stop();
	}
	// and delete the shared pointers
	physical_switches.clear();

	// Stop all of the slices
	for( Slice& s : slices ) s.stop();
	// and delete all the virtual switch shared pointers
	slices.clear();
}

void Hypervisor::calculate_routes() {
	// Reset all switches to start values
	for( auto& phy_switch : physical_switches ) {
		phy_switch.second->reset_distances();
	}

	// Execute the floyd-warshall algorithm, naming convention taken
	// from https://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm
	for( auto &k : physical_switches ) {
		for( auto &i : physical_switches ) {
			for( auto &j : physical_switches ) {
				int dist_i_k = i.second->get_distance(k.first);
				int dist_k_j = k.second->get_distance(j.first);
				int dist_i_j = i.second->get_distance(j.first);
				if( dist_i_k + dist_k_j < dist_i_j ) {
					i.second->set_distance( j.first, dist_i_k + dist_i_j );
					i.second->set_next( j.first, i.second->get_next(k.first) );
				}
			}
		}
	}

	// Let all the virtual switches check if they should go online/down
	for( Slice& s : slices ) s.check_online();

	// Let all physical switches check if the dynamic forwarding rules need to update
	for( auto &ps : physical_switches ) ps.second->update_dynamic_rules();

	// Write the new topology in dot format to a file
	// TODO Remove this debugging info
	std::ofstream topo_file("topo.dot");
	print_topology(topo_file);
}

void Hypervisor::print_topology(std::ostream& os) {
	os << "graph {\n";
	for( const auto &ps : physical_switches ) {
		int id = ps.second->get_id();
		os << "\t" << id << " -- { ";
		for( const auto &p : ps.second->get_ports() ) {
			if( p.second.link != nullptr ) {
				int other_id = p.second.link->get_other_switch_id(id);
				// Only add each link once
				if( id < other_id ) {
					os << other_id << " ";
				}
			}
		}
		os << "}\n";
	}
	os << "}\n" << std::flush;
}

void Hypervisor::start_listening( int port ) {
	// Bind the switch_acceptor to the configured port
	switch_acceptor.open( boost::asio::ip::tcp::v4() );
	switch_acceptor.bind(
		boost::asio::ip::tcp::endpoint(
			boost::asio::ip::tcp::v4(),
			port));
	switch_acceptor.listen();
}

void Hypervisor::load_configuration( std::string filename ) {
	// Read the configuration file into memory
	boost::property_tree::ptree config_tree;
	boost::property_tree::json_parser::read_json( filename, config_tree );

	// Start listening for physical switches
	start_listening(config_tree.get<int>("switch_endpoint_port"));

	// Create the internal structure
	for( const auto &slice_pair : config_tree.get_child("slices") ) {
		auto& slice_ptree = slice_pair.second;

		int max_rate   = slice_ptree.get<int>("max_rate");
		std::string ip = slice_ptree.get_child("controller").get<std::string>("ip");
		int port       = slice_ptree.get_child("controller").get<int>("port");
		slices.emplace_back( slices.size(), max_rate, ip, port, this );

		Slice& slice = slices.back();

		for( const auto &virtual_switch_pair : slice_ptree.get_child("virtual_switches") ) {
			auto& virtual_switch_ptree = virtual_switch_pair.second;

			uint64_t datapath_id = virtual_switch_ptree.get<uint64_t>("datapath_id");
			slice.add_new_virtual_switch(
					switch_acceptor.get_io_service(),
					datapath_id);

			VirtualSwitch::pointer virtual_switch =
				slice.get_virtual_switch_by_datapath_id(datapath_id);

			for( const auto &port_pair : virtual_switch_ptree.get_child("ports") ) {
				auto& port_ptree = port_pair.second;

				uint32_t virtual_port         =
					port_ptree.get<uint32_t>("virtual_port");
				uint64_t physical_datapath_id =
					port_ptree.get<uint64_t>("physical_datapath_id");
				uint32_t physical_port        =
					port_ptree.get<uint32_t>("physical_port");

				virtual_switch->add_port(
					virtual_port,
					physical_datapath_id,
					physical_port);
			}
		}
	}
}
