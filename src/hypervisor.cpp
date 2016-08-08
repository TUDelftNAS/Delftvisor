#include "hypervisor.hpp"

#include <iostream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/bind.hpp>

Hypervisor::Hypervisor( boost::asio::io_service& io ) :
	signals(io, SIGINT, SIGTERM),
	switch_acceptor(io) {
}

void Hypervisor::handle_signals(
	const boost::system::error_code& error,
	int signal_number
) {
	if( !error ) {
		stop();
	}
}

void Hypervisor::start_accept() {
	PhysicalSwitch::pointer new_physical_switch = PhysicalSwitch::create(switch_acceptor.get_io_service());

	switch_acceptor.async_accept(
		new_physical_switch->get_socket(),
		boost::bind(
			&Hypervisor::handle_accept,
			this,
			boost::asio::placeholders::error,
			new_physical_switch));
}

void Hypervisor::handle_accept( const boost::system::error_code& error, PhysicalSwitch::pointer physical_switch ) {
	if( !error ) {
		// Add the physical switch to the list
		new_physical_switches.push_back( physical_switch );

		// And start the physical switch
		physical_switch->start();

		// Start waiting for the next connection
		start_accept();
	}
	else {
		std::cerr << "Something went wrong while accepting a connection: " << error.message() << std::endl;
	}
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
	std::cout << "Stopping Hypervisor" << std::endl;

	// Cancel the signal handler if it is still running
	signals.cancel();

	// Stop accepting new switch connections, this also
	// cancels all pending operations on the acceptor
	switch_acceptor.close();

	// Stop all physical switches
	for( auto s : new_physical_switches ) s->stop();
	// and delete the shared pointers
	new_physical_switches.clear();

	// Stop all of the slices
	for( Slice s : slices ) s.stop();
	// and delete all the virtual switch shared pointers
	slices.clear();
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
	/*
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
	*/
}
