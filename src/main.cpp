#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include "hypervisor.hpp"

// The arguments to be parsed
int num_threads;
std::string configuration_file;

/// Parse the command line arguments
/**
 * The options are stored in global variables.
 * \param argc The argument count as passed to main
 * \param argv The arguments as passed to main
 * \return If the parsing was successful
 */
bool parse_arguments(int argc, char* argv[]) {
	// Define the command line options
	boost::program_options::options_description desc("Allowed options");
	desc.add_options()
		("help,h", "Produce this help message")
		("config_file,f", boost::program_options::value<std::string>(&configuration_file), "Configuration file path")
		("num_threads,t", boost::program_options::value<int>(&num_threads)->default_value(1), "Amount of threads to spawn");

	// A positional option is used so you don't have to specify
	// explicitly that the first argument is the config file
	boost::program_options::positional_options_description p;
	p.add("config_file", 1);

	// Try to parse the command line options
	boost::program_options::variables_map vm;
	boost::program_options::store(
		boost::program_options::command_line_parser(argc, argv)
			.options(desc)
			.positional(p)
			.run(),
		vm);
	try {
		boost::program_options::notify(vm);
	}
	catch( boost::program_options::unknown_option& e ) {
		std::cerr << e.what() << std::endl << std::endl;
		std::cout << desc << std::endl;
		return false;
	}
	catch( boost::program_options::too_many_positional_options_error& ) {
		std::cerr << "Too many arguments passed" << std::endl << std::endl;
		std::cout << desc << std::endl;
		return false;
	}

	// Print a help message if needed
	if( vm.count("help")) {
		std::cout << desc << std::endl;
		return false;
	}

	// Check if a config file was passed
	if( !vm.count("config_file") ) {
		std::cerr << "A config file must be passed" << std::endl << std::endl;
		std::cout << desc << std::endl;
		return false;
	}

	// Check that the amount of threads is valid
	if( num_threads < 1 ) {
		std::cerr << "Amount of threads must be positive" << std::endl;
		return false;
	}

	// Everything went ok
	return true;
}

int main(int argc, char* argv[]) {
	// Try to parse the arguments
	if( !parse_arguments(argc, argv) )
		return 1;

	// Create an io_service
	boost::asio::io_service io;

	// Startup the hypervisor
	Hypervisor h( io );
	try {
		h.load_configuration( configuration_file );
	}
	catch( ... ) {
		std::cerr << "Problem in configuration file " << configuration_file << std::endl;
		return 1;
	}
	h.start();

	// Create the threads
	std::vector<boost::thread> threads;
	for( size_t i=0; i<num_threads; ++i ) {
		threads.emplace_back(boost::bind(&boost::asio::io_service::run, &io));
	}

	// Join the threads again when all work is done
	for( size_t i=0; i<num_threads; ++i ) {
		threads[i].join();
	}

	std::cout << "All done!" << std::endl;

	return 0;
}
