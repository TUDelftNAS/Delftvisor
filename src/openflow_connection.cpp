#include "openflow_connection.hpp"

#include <iostream>

#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>

OpenflowConnection::OpenflowConnection( boost::asio::ip::tcp::socket& socket ) :
	// Construct the socket of this connection from an existing socket
	socket(std::move(socket)),
	echo_timer(socket.get_io_service(),boost::posix_time::milliseconds(0)),
	echo_received(true) {
}

OpenflowConnection::OpenflowConnection( boost::asio::io_service& io ) :
	// Construct a new socket
	socket(io),
	echo_timer(io,boost::posix_time::milliseconds(0)),
	echo_received(true) {
}

void OpenflowConnection::start() {
	// Start listening for openflow messages
	start_receive_message();

	// Start sending echo messages over this connection
	schedule_echo_message();

	// Send a hello message to the other side,
	// the hello element bitmap is not mandatory
	fluid_msg::of13::Hello hello_msg(get_next_xid());
	send_message(hello_msg);
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
		BOOST_LOG_TRIVIAL(error) << "Error while receiving packet header: " << error.message();
	}
}

void OpenflowConnection::receive_body(const boost::system::error_code& error, std::size_t bytes_transferred) {
	if( !error ) {
		// Extract the type of the message
		uint8_t type = message_buffer[1];

		// Unpack the message into a libfluid object and
		// call the handle function.
		switch( type ) {
		case fluid_msg::of13::OFPT_HELLO: {
			fluid_msg::of13::Hello hello_msg;
			hello_msg.unpack(&message_buffer[0]);
			handle_hello(hello_msg); }
			break;

		case fluid_msg::of13::OFPT_ERROR: {
			fluid_msg::of13::Error error_msg;
			error_msg.unpack(&message_buffer[0]);
			handle_error(error_msg); }
			break;

		case fluid_msg::of13::OFPT_ECHO_REQUEST: {
			fluid_msg::of13::EchoRequest echo_request;
			echo_request.unpack(&message_buffer[0]);
			handle_echo_request(echo_request); }
			break;

		case fluid_msg::of13::OFPT_ECHO_REPLY: {
			fluid_msg::of13::EchoReply echo_reply;
			echo_reply.unpack(&message_buffer[0]);
			handle_echo_reply(echo_reply); }
			break;
		}
	}
	else {
		BOOST_LOG_TRIVIAL(error) << "Error while receiving packet body: " << error.message();
	}

	// Start waiting for the next message
	start_receive_message();
}

void OpenflowConnection::send_message(fluid_msg::OFMsg& message) {
	// Get the lock for the message queue
	boost::lock_guard<boost::mutex> guard(send_queue_mutex);

	// Check if this connection is currently sending a message, if it
	// is not we need to start up the chain of send message calls
	// otherwise it will automatically send. If a message is currently
	// being send there will be a message in the queue.
	bool startup_send_chain = (send_queue.size()==0);

	// Create the buffer from the message and 
	uint8_t* buffer = message.pack();
	std::vector<uint8_t> msg( message.length() );
	for( size_t i=0; i<msg.size(); ++i ) msg[i]=buffer[i];
	fluid_msg::OFMsg::free_buffer( buffer );

	// Add the message to the queue
	send_queue.push( std::move(msg) );

	// Startup the send chain if needed
	if( startup_send_chain ) send_message_queue_head();
}

void OpenflowConnection::send_message_queue_head() {
	BOOST_LOG_TRIVIAL(trace) << "Sending message from " << *this << ", current queue length: " << send_queue.size();

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
		BOOST_LOG_TRIVIAL(error) << "Problem sending message on " << *this << ": " << error.message();
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
		BOOST_LOG_TRIVIAL(error) << "Missed echo message";
	}

	// Send the echo message
	fluid_msg::of13::EchoRequest echo_msg(get_next_xid());
	send_message(echo_msg);
	echo_received = false;

	// Schedule the next echo message to be send
	schedule_echo_message();
}

uint32_t OpenflowConnection::get_next_xid() {
	static uint32_t next_xid=0;

	return next_xid++;
}

void OpenflowConnection::handle_hello(fluid_msg::of13::Hello& hello_message) {
	// Try to figure out if this connection can handle openflow 1.3
	if( hello_message.elements().front().bitmaps().front() & (1<<3) == 0 ) {
		BOOST_LOG_TRIVIAL(error) << "Hello received with wrong openflow version";

		// Send an error message
		fluid_msg::of13::Error error_msg(
			hello_message.xid(),
			fluid_msg::of13::OFPET_HELLO_FAILED,
			fluid_msg::of13::OFPHFC_INCOMPATIBLE);
		send_message( error_msg );
	}
	else {
		BOOST_LOG_TRIVIAL(info) << "Hello received";
	}
}

void OpenflowConnection::handle_echo_request(fluid_msg::of13::EchoRequest& echo_request_message) {
	// Send the echo response
	fluid_msg::of13::EchoReply(echo_request_message.xid());
}

void OpenflowConnection::handle_echo_reply(fluid_msg::of13::EchoReply& echo_reply_message) {
	echo_received = true;
}

void OpenflowConnection::handle_experimenter(fluid_msg::of13::Experimenter& experimenter_message) {
	BOOST_LOG_TRIVIAL(error) << "Received experimenter on " << *this;

	// Send an error message
	fluid_msg::of13::Error error_msg(
		experimenter_message.xid(),
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_EXPERIMENTER);
	send_message( error_msg );
}

/// Explain how to print these objects
std::ostream& operator<<(std::ostream& os, const OpenflowConnection& con) {
	con.print_to_stream(os);
	return os;
}
