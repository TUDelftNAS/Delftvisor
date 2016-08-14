#include <iostream>
#include <string>

#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include "hypervisor.hpp"

/// The amount of threads to spawn
int num_threads;
/// The filename of the configuration file
std::string configuration_file;
/// The log level
std::string log_level;
/// The log filename
std::string log_file;

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
		("num_threads,t", boost::program_options::value<int>(&num_threads)->default_value(1), "Amount of threads to spawn")
		("log_level,l", boost::program_options::value<std::string>(&log_level)->default_value("error"))
		("log_file", boost::program_options::value<std::string>(&log_file));

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

	// Set the correct log level
	boost::log::trivial::severity_level level;
	if( log_level == "trace" ) {
		level = boost::log::trivial::trace;
	}
	else if( log_level == "debug" ) {
		level = boost::log::trivial::debug;
	}
	else if( log_level == "info" ) {
		level = boost::log::trivial::info;
	}
	else if( log_level == "warning" ) {
		level = boost::log::trivial::warning;
	}
	else if( log_level == "error" ) {
		level = boost::log::trivial::warning;
	}
	else if( log_level == "fatal" ) {
		level = boost::log::trivial::fatal;
	}
	else {
		std::cerr << "Unknown log level \"" << log_level << "\"" << std::endl;
		return false;
	}
	boost::log::core::get()->set_filter(
		boost::log::trivial::severity >= level
	);

	// Tell the logger to store timestamps and the like
	boost::log::add_common_attributes();

	// Tell the logger to write to file if needed
	if( log_file != "" ) {
		boost::log::add_file_log(
			boost::log::keywords::file_name = log_file,
			boost::log::keywords::format = "[%TimeStamp%]: %Message%"
		);
	}

	// Check that the amount of threads is valid
	if( num_threads < 1 ) {
		std::cerr << "Amount of threads must be positive" << std::endl;
		return false;
	}
	else if( num_threads > 1 ) {
		std::cerr << "Using more than 1 threads is still experimental" << std::endl;
	}

	// Everything went ok
	return true;
}

/// Main entry point of the program
/**
 * \param argc Amount of command line arguments passed
 * \param argv Argument values passed
 * \return Program exit code
 */
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

	BOOST_LOG_TRIVIAL(info) << "Started up hypervisor";

	// Create the threads
	std::vector<boost::thread> threads;
	for( size_t i=0; i<(num_threads-1); ++i ) {
		threads.emplace_back(boost::bind(&boost::asio::io_service::run, &io));
	}

	BOOST_LOG_TRIVIAL(info) << "Started " << num_threads << " threads";
	// Also use this thread to service handlers
	io.run();

	// Join the threads again when all work is done
	for( size_t i=0; i<(num_threads-1); ++i ) {
		threads[i].join();
	}

	BOOST_LOG_TRIVIAL(info) << "Joined " << num_threads << " threads";

	return 0;
}
