#include <iostream>

#include "hypervisor.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>

int main(int argc, char* argv[]) {
	// Create an io_service
	boost::asio::io_service io;

	Hypervisor h( io );
	h.start();

	// Create a set number of threads
	const int num_threads=8;
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
