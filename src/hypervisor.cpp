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

void Hypervisor::start() {
	// Register the handler for signals
	signals.async_wait(boost::bind(
		&Hypervisor::handle_signals,
		this,
		boost::asio::placeholders::error,
		boost::asio::placeholders::signal_number));

	// Register the acceptor for switch connections

	// Start all of the slices
	for( Slice s : slices ) s.start();
}

void Hypervisor::stop() {
	std::cout << "Stopping Hypervisor!" << std::endl;

	// Cancel the signal handler if it is still running
	signals.cancel();

	// Stop accepting new switch connections
	switch_acceptor.close();

	// Stop all physical switches

	// Stop all of the slices
	for( Slice s : slices ) s.stop();
}

void Hypervisor::load_configuration( std::string filename ) {
	// Read the configuration file into memory
	boost::property_tree::ptree config_tree;
	boost::property_tree::json_parser::read_json( filename, config_tree );

	// Bind the switch_acceptor to the configured port
	switch_acceptor.open( boost::asio::ip::tcp::v4() );
	switch_acceptor.bind(
		boost::asio::ip::tcp::endpoint(
			boost::asio::ip::tcp::v4(),
			config_tree.get<int>("switch_endpoint_port")));

	// Create the internal structure
	for( const auto &slice_pair : config_tree.get_child("slices") ) {
		auto slice_ptree = slice_pair.second;

		slices.emplace_back( slices.size(), slice_ptree.get<int>("max_rate") );
		std::string ip = slice_ptree.get_child("controller").get<std::string>("ip");
		int port       = slice_ptree.get_child("controller").get<int>("port");

		std::cout << "on address " << ip << ":" << port << std::endl;

		for( const auto &virtual_switch_pair : slice_ptree.get_child("virtual_switches") ) {
			auto virtual_switch_ptree = virtual_switch_pair.second;

			int64_t datapath_id = virtual_switch_ptree.get<int64_t>("datapath_id");

			std::cout << "\tVirtual switch (dpid=" << datapath_id << ")" << std::endl;

			for( const auto &physical_port_pair : virtual_switch_ptree.get_child("physical_ports") ) {
				auto physical_port_ptree = physical_port_pair.second;

				int64_t other_datapath_id = physical_port_ptree.get<int64_t>("datapath_id");

				for( const auto &port_pair : physical_port_ptree.get_child("ports") ) {
					int port = port_pair.second.get<int>("");

					std::cout << "\t\tPort (" << other_datapath_id << ", " << port << ")" << std::endl;
				}
			}
		}
	}
}
