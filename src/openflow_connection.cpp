#include "openflow_connection.hpp"

#include <iostream>

#include <boost/bind.hpp>

boost::asio::ip::tcp::socket& OpenflowConnection::get_socket() {
	return socket;
}

OpenflowConnection::OpenflowConnection( boost::asio::io_service& io ) :
	socket(io),
	echo_timer(io,boost::posix_time::milliseconds(0)) {
}

void OpenflowConnection::start() {
	// Start listening for openflow messages
	start_receive_message();

	// Start sending echo messages over this connection
	send_echo_message();
}

void OpenflowConnection::stop() {
	// Closing the socket stops all socket actions
	socket.close();

	// Stop sending echo messages over this connection
	echo_timer.cancel();
}

void OpenflowConnection::start_receive_message() {
	// Make sure the message buffer is large enough
	if( message_buffer.size() < 8 ) message_buffer.resize(8);

	// Receive the header of the openflow message
	boost::asio::async_read(
		socket,
		boost::asio::buffer(&message_buffer[0], 8),
		boost::bind(
			&OpenflowConnection::receive_header,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void OpenflowConnection::receive_header(const boost::system::error_code& error, std::size_t bytes_transferred) {
	if( !error ) {
		// Extract the length of the total packet from the received header
		size_t length = message_buffer[2]*256+message_buffer[3];

		// Make sure the message buffer is large enough
		if( message_buffer.size() < length ) message_buffer.resize(length);

		// Receive the body of the message
		boost::asio::async_read(
			socket,
			boost::asio::buffer(&message_buffer[8], length-8),
			boost::bind(
				&OpenflowConnection::receive_body,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
	}
	else {
		std::cerr << "Error while receiving packet header : " << error.message() << std::endl;
	}
}

void OpenflowConnection::receive_body(const boost::system::error_code& error, std::size_t bytes_transferred) {
	if( !error ) {
		// Unpack the message from the buffer

		// If this message is echo just sent it back, otherwise:
		// Handle the message
		handle_message();

		// Start waiting for the next message
		start_receive_message();
	}
	else {
		std::cerr << "Error while receiving packet body : " << error.message() << std::endl;
	}
}

void OpenflowConnection::send_message(fluid_msg::OFMsg* message) {
	// Get the lock for the message queue
	boost::lock_guard<boost::mutex> guard(send_queue_mutex);

	// Check if this connection is currently sending a message, if it
	// is not we need to start up the chain of send message calls
	// otherwise it will automatically send. If a message is currently
	// being send there will be a message in the queue.
	bool startup_send_chain = (send_queue.size()==0);

	uint8_t* buffer = message->pack();
	std::vector<uint8_t> msg( message->length() );
	for( size_t i=0; i<msg.size(); ++i ) msg[i]=buffer[i];
	fluid_msg::OFMsg::free_buffer( buffer );

	// Add the message to the queue
	// TODO Should this use std::move?
	send_queue.push( msg );

	// Startup the send chain if needed
	if( startup_send_chain ) send_message_queue_head();
}

void OpenflowConnection::send_message_queue_head() {
	// The function calling this function should own the
	// message_queue_mutex. Send the entirety of the message
	// in the front of the queue. Pointer and references
	// to elements of the queue should stay valid while
	// pushing and popping elements..
	boost::asio::async_write(
		socket,
		boost::asio::buffer(send_queue.front()),
		boost::bind(
			&OpenflowConnection::handle_send_message,
			shared_from_this(),
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
}

void OpenflowConnection::handle_send_message(const boost::system::error_code& error, std::size_t bytes_transferred) {
	// Get the lock for the message queue
	boost::lock_guard<boost::mutex> guard(send_queue_mutex);

	// Remove the element of the queue that was just sent
	send_queue.pop();

	if( !error ) {
		// If there are more elements in the queue to send,
		// send them
		if( send_queue.size() != 0 ) {
			send_message_queue_head();
		}
	}
	else {
		std::cerr << "Problem transferring message : " << error.message() << std::endl;
	}
}

void OpenflowConnection::schedule_echo_message() {
	// Set the echo timer to fire after a small period
	echo_timer.expires_at(echo_timer.expires_at() + boost::posix_time::milliseconds(1000));
	echo_timer.async_wait(boost::bind(&OpenflowConnection::send_echo_message,shared_from_this()));
}

void OpenflowConnection::send_echo_message() {
	// If this connection is closed don't send the message
	// and don't schedule the next message. This connection
	// has likely been closed.
	if( !socket.is_open() ) return;

	// TODO What to do if the echo wasn't answered?
	if( !echo_received ) {
		//std::cerr << "Missed echo message" << std::endl;
	}

	// Send the echo message
	fluid_msg::of13::EchoRequest echo_msg(get_next_xid());
	send_message(&echo_msg);
	echo_received = false;
}

uint32_t OpenflowConnection::get_next_xid() {
	static uint32_t next_xid=0;

	return next_xid++;
}
