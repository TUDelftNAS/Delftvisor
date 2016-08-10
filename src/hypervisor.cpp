#include "hypervisor.hpp"

#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include <boost/make_shared.hpp>

Hypervisor::Hypervisor( boost::asio::io_service& io ) :
	signals(io, SIGINT, SIGTERM),
	switch_acceptor(io),
	next_physical_switch_id(0) {
}

void Hypervisor::handle_signals(
	const boost::system::error_code& error,
	int signal_number
) {
	if( !error ) {
		BOOST_LOG_TRIVIAL(trace) << "Received signal " << signal_number;
		stop();
	}
	else {
		BOOST_LOG_TRIVIAL(error) << "Error while receiving signal: " << error.message();
	}
}

void Hypervisor::start_accept() {
	boost::shared_ptr<boost::asio::ip::tcp::socket> new_socket = boost::make_shared<boost::asio::ip::tcp::socket>(switch_acceptor.get_io_service());

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
		// Calculate the next switch id
		int id = next_physical_switch_id++;

		// Add the physical switch to the list
		physical_switches.emplace(
			id,
			boost::make_shared<PhysicalSwitch>(
				*socket,
				id,
				this));

		// And start the physical switch
		physical_switches[id]->start();

		// Start waiting for the next connection
		start_accept();
	}
	else {
		BOOST_LOG_TRIVIAL(error) << "Something went wrong while accepting a connection: " << error.message();
	}
}

void Hypervisor::register_physical_switch(uint64_t datapath_id, int switch_id) {
	datapath_id_to_switch_id[datapath_id] = switch_id;
}

void Hypervisor::unregister_physical_switch(int switch_id) {
	physical_switches.erase(switch_id);
}
void Hypervisor::unregister_physical_switch(uint64_t datapath_id, int switch_id) {
	datapath_id_to_switch_id.erase(datapath_id);
	physical_switches.erase(switch_id);
}

PhysicalSwitch::pointer Hypervisor::get_physical_switch(int switch_id) {
	return physical_switches[switch_id];
}
PhysicalSwitch::pointer Hypervisor::get_physical_switch_by_datapath_id(uint64_t datapath_id) {
	return physical_switches[datapath_id_to_switch_id[datapath_id]];
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
	for( Slice s : slices ) s.start();
}

void Hypervisor::stop() {
	BOOST_LOG_TRIVIAL(trace) << "Stopping Hypervisor";

	// Cancel the signal handler if it is still running
	signals.cancel();

	// Stop accepting new switch connections, this also
	// cancels all pending operations on the acceptor
	switch_acceptor.close();

	// Stop all physical switches
	for( auto s : physical_switches ) s.second->stop();
	// and delete the shared pointers
	physical_switches.clear();

	// Stop all of the slices
	for( Slice s : slices ) s.stop();
	// and delete all the virtual switch shared pointers
	slices.clear();
}

void Hypervisor::calculate_routes() {
	// Reset all switches to start values
	for( auto phy_switch : physical_switches ) {
		phy_switch.second->reset_distances();
	}

	// Execute the floyd-warshall algorithm, naming convention taken
	// from https://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm
	for( auto k : physical_switches ) {
		for( auto i : physical_switches ) {
			for( auto j : physical_switches ) {
				int dist_i_k = i.second->get_distance(k.first);
				int dist_k_j = k.second->get_distance(j.first);
				int dist_i_j = i.second->get_distance(j.first);
				if( dist_i_k + dist_k_j < dist_i_j ) {
					i.second->set_distance( j.first, dist_i_k + dist_i_j );
					i.second->set_next( j.first, i.second->get_distance(k.first) );
				}
			}
		}
	}
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
		auto slice_ptree = slice_pair.second;

		int max_rate   = slice_ptree.get<int>("max_rate");
		std::string ip = slice_ptree.get_child("controller").get<std::string>("ip");
		int port       = slice_ptree.get_child("controller").get<int>("port");
		slices.emplace_back( slices.size(), max_rate, ip, port );

		for( const auto &virtual_switch_pair : slice_ptree.get_child("virtual_switches") ) {
			auto virtual_switch_ptree = virtual_switch_pair.second;

			int64_t datapath_id = virtual_switch_ptree.get<int64_t>("datapath_id");

			slices.back().add_new_virtual_switch(switch_acceptor.get_io_service(), datapath_id);

			for( const auto &physical_port_pair : virtual_switch_ptree.get_child("physical_ports") ) {
				auto physical_port_ptree = physical_port_pair.second;

				int64_t other_datapath_id = physical_port_ptree.get<int64_t>("datapath_id");

				for( const auto &port_pair : physical_port_ptree.get_child("ports") ) {
					int port = port_pair.second.get<int>("");

					// TODO Add the ports to the virtual switch
				}
			}
		}
	}
}
