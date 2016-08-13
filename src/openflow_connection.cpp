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

void OpenflowConnection::handle_network_error(
		const boost::system::error_code& error) {
	switch( error.value() ) {
	case boost::asio::error::operation_aborted:
		// Do nothing, this probably means that stop
		// was called on this connection.
		break;
	case boost::asio::error::connection_aborted:
	case boost::asio::error::connection_reset:
	case boost::asio::error::eof:
		// If the other side gives up stop this connection
		stop();
		BOOST_LOG_TRIVIAL(trace) << *this << " connection was " << error.message();
		break;
	default:
		BOOST_LOG_TRIVIAL(error) << *this << " has network problem: " << error.message();
	}
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

void OpenflowConnection::receive_header(
		const boost::system::error_code& error,
		std::size_t bytes_transferred) {
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
		handle_network_error(error);
	}
}

template<class libfluid_message,void (OpenflowConnection::*handle_function)(libfluid_message&)>
void OpenflowConnection::receive_message() {
	// Try to unpack the message
	libfluid_message message;
	fluid_msg::of_error error = message.unpack(&message_buffer[0]);

	// If an error occured just forward it to
	if( error ) {
		fluid_msg::of13::Error error_message(
			message.xid(),
			fluid_msg::of_error_type(error),
			fluid_msg::of_error_code(error));
		this->send_message(error_message);

		BOOST_LOG_TRIVIAL(error) << *this << " had an error parsing a message";
	}
	else {
		// Handle the correctly parsed message
		(this->*handle_function)(message);
	}
}

void OpenflowConnection::receive_body(
		const boost::system::error_code& error,
		std::size_t bytes_transferred) {
	// Handle the error if necessary
	if( error ) {
		handle_network_error(error);
		return;
	}

	// Extract the type of the message
	uint8_t type = message_buffer[1];

	// Unpack the message into a libfluid object and
	// call the handle function. This switch statement
	// is stupid long but I couldn't find an function
	// in libfluid_msg that did it for me.
	switch( type ) {
	case fluid_msg::of13::OFPT_HELLO:
		receive_message<
			fluid_msg::of13::Hello,
			&OpenflowConnection::handle_hello>();
		break;

	case fluid_msg::of13::OFPT_ERROR:
		receive_message<
			fluid_msg::of13::Error,
			&OpenflowConnection::handle_error>();
		break;

	case fluid_msg::of13::OFPT_ECHO_REQUEST:
		receive_message<
			fluid_msg::of13::EchoRequest,
			&OpenflowConnection::handle_echo_request>();
		break;

	case fluid_msg::of13::OFPT_ECHO_REPLY:
		receive_message<
			fluid_msg::of13::EchoReply,
			&OpenflowConnection::handle_echo_reply>();
		break;

	case fluid_msg::of13::OFPT_EXPERIMENTER:
		receive_message<
			fluid_msg::of13::Experimenter,
			&OpenflowConnection::handle_experimenter>();
		break;

	case fluid_msg::of13::OFPT_FEATURES_REQUEST:
		receive_message<
			fluid_msg::of13::FeaturesRequest,
			&OpenflowConnection::handle_features_request>();
		break;

	case fluid_msg::of13::OFPT_FEATURES_REPLY:
		receive_message<
			fluid_msg::of13::FeaturesReply,
			&OpenflowConnection::handle_features_reply>();
		break;

	case fluid_msg::of13::OFPT_GET_CONFIG_REQUEST:
		receive_message<
			fluid_msg::of13::GetConfigRequest,
			&OpenflowConnection::handle_config_request>();
		break;

	case fluid_msg::of13::OFPT_GET_CONFIG_REPLY:
		receive_message<
			fluid_msg::of13::GetConfigReply,
			&OpenflowConnection::handle_config_reply>();
		break;

	case fluid_msg::of13::OFPT_SET_CONFIG:
		receive_message<
			fluid_msg::of13::SetConfig,
			&OpenflowConnection::handle_set_config>();
		break;

	case fluid_msg::of13::OFPT_BARRIER_REQUEST:
		receive_message<
			fluid_msg::of13::BarrierRequest,
			&OpenflowConnection::handle_barrier_request>();
		break;

	case fluid_msg::of13::OFPT_BARRIER_REPLY:
		receive_message<
			fluid_msg::of13::BarrierReply,
			&OpenflowConnection::handle_barrier_reply>();
		break;

	case fluid_msg::of13::OFPT_PACKET_IN:
		receive_message<
			fluid_msg::of13::PacketIn,
			&OpenflowConnection::handle_packet_in>();
		break;

	case fluid_msg::of13::OFPT_PACKET_OUT:
		receive_message<
			fluid_msg::of13::PacketOut,
			&OpenflowConnection::handle_packet_out>();
		break;

	case fluid_msg::of13::OFPT_FLOW_REMOVED:
		receive_message<
			fluid_msg::of13::FlowRemoved,
			&OpenflowConnection::handle_flow_removed>();
		break;

	case fluid_msg::of13::OFPT_PORT_STATUS:
		receive_message<
			fluid_msg::of13::PortStatus,
			&OpenflowConnection::handle_port_status>();
		break;

	case fluid_msg::of13::OFPT_FLOW_MOD:
		receive_message<
			fluid_msg::of13::FlowMod,
			&OpenflowConnection::handle_flow_mod>();
		break;

	case fluid_msg::of13::OFPT_GROUP_MOD:
		receive_message<
			fluid_msg::of13::GroupMod,
			&OpenflowConnection::handle_group_mod>();
		break;

	case fluid_msg::of13::OFPT_PORT_MOD:
		receive_message<
			fluid_msg::of13::PortMod,
			&OpenflowConnection::handle_port_mod>();
		break;

	case fluid_msg::of13::OFPT_TABLE_MOD:
		receive_message<
			fluid_msg::of13::TableMod,
			&OpenflowConnection::handle_table_mod>();
		break;

	case fluid_msg::of13::OFPT_METER_MOD:
		receive_message<
			fluid_msg::of13::MeterMod,
			&OpenflowConnection::handle_meter_mod>();
		break;

	case fluid_msg::of13::OFPT_MULTIPART_REQUEST:
		receive_message<
			fluid_msg::of13::MultipartRequest,
			&OpenflowConnection::handle_multipart_request>();
		break;

	case fluid_msg::of13::OFPT_MULTIPART_REPLY:
		receive_message<
			fluid_msg::of13::MultipartReply,
			&OpenflowConnection::handle_multipart_reply>();
		break;

	case fluid_msg::of13::OFPT_QUEUE_GET_CONFIG_REQUEST:
		receive_message<
			fluid_msg::of13::QueueGetConfigRequest,
			&OpenflowConnection::handle_queue_config_request>();
		break;

	case fluid_msg::of13::OFPT_QUEUE_GET_CONFIG_REPLY:
		receive_message<
			fluid_msg::of13::QueueGetConfigReply,
			&OpenflowConnection::handle_queue_config_reply>();
		break;

	case fluid_msg::of13::OFPT_ROLE_REQUEST:
		receive_message<
			fluid_msg::of13::RoleRequest,
			&OpenflowConnection::handle_role_request>();
		break;

	case fluid_msg::of13::OFPT_ROLE_REPLY:
		receive_message<
			fluid_msg::of13::RoleReply,
			&OpenflowConnection::handle_role_reply>();
		break;

	case fluid_msg::of13::OFPT_GET_ASYNC_REQUEST:
		receive_message<
			fluid_msg::of13::GetAsyncRequest,
			&OpenflowConnection::handle_get_async_request>();
		break;

	case fluid_msg::of13::OFPT_GET_ASYNC_REPLY:
		receive_message<
			fluid_msg::of13::GetAsyncReply,
			&OpenflowConnection::handle_get_async_reply>();
		break;

	case fluid_msg::of13::OFPT_SET_ASYNC:
		receive_message<
			fluid_msg::of13::SetAsync,
			&OpenflowConnection::handle_set_async>();
		break;

	default:
		BOOST_LOG_TRIVIAL(error) << *this << " received unknown message with type " << type;
		break;
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

	// Create the buffer from the message, copy it into a
	// vector and free the buffer again.
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

void OpenflowConnection::handle_send_message(
		const boost::system::error_code& error,
		std::size_t bytes_transferred) {
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
		handle_network_error(error);
	}
}

void OpenflowConnection::schedule_echo_message() {
	// Set the echo timer to fire after a small period
	echo_timer.expires_from_now(
		boost::posix_time::milliseconds(1000));
	echo_timer.async_wait(
		boost::bind(
			&OpenflowConnection::send_echo_message,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void OpenflowConnection::send_echo_message(const boost::system::error_code& error) {
	// If this connection is closed don't send the message
	// and don't schedule the next message. This connection
	// has likely been closed.
	if( error.value() == boost::asio::error::operation_aborted ) {
		BOOST_LOG_TRIVIAL(trace) << *this << " echo timer cancelled";
		return;
	}
	else if( error ) {
		BOOST_LOG_TRIVIAL(error) << *this << " echo timer error: " << error.message();
		return;
	}

	// TODO What to do if the echo wasn't answered?
	if( !echo_received ) {
		BOOST_LOG_TRIVIAL(error) << *this << " missed echo message";
	}

	// Send the echo message
	fluid_msg::of13::EchoRequest echo_msg(get_next_xid());
	send_message(echo_msg);
	echo_received = false;
	BOOST_LOG_TRIVIAL(trace) << *this << " send echo request";

	// Schedule the next echo message to be send
	schedule_echo_message();
}

uint32_t OpenflowConnection::get_next_xid() {
	static uint32_t next_xid=0;

	return next_xid++;
}

void OpenflowConnection::handle_hello(fluid_msg::of13::Hello& hello_message) {
	// Try to figure out if this connection can handle openflow 1.3, it
	// doesn't really matter though since if it is not openflow 1.3 the
	// hello message doesn't parse
	if(
		(hello_message.elements().size()>0 && hello_message.elements().front().bitmaps().front() & (1<<3) == 0) ||
		(hello_message.elements().size()==0 && hello_message.version()!=fluid_msg::of13::OFP_VERSION)
	) {
		BOOST_LOG_TRIVIAL(error) << *this << " received Hello with wrong openflow version";

		// Send an error message
		fluid_msg::of13::Error error_msg(
			hello_message.xid(),
			fluid_msg::of13::OFPET_HELLO_FAILED,
			fluid_msg::of13::OFPHFC_INCOMPATIBLE);
		send_message( error_msg );

		// Close the connection, this isn't going to work
		stop();
	}
	else {
		BOOST_LOG_TRIVIAL(info) << *this << " received Hello";
	}
}

void OpenflowConnection::handle_echo_request(
		fluid_msg::of13::EchoRequest& echo_request_message) {
	// Create the echo reply
	fluid_msg::of13::EchoReply echo_reply_message(echo_request_message.xid());
	echo_reply_message.data(
		echo_request_message.data(),
		echo_request_message.data_len());

	// Send the echo reply
	this->send_message(echo_reply_message);

	BOOST_LOG_TRIVIAL(trace) << *this << " responded to echo request";
}

void OpenflowConnection::handle_echo_reply(
		fluid_msg::of13::EchoReply& echo_reply_message) {
	echo_received = true;
	BOOST_LOG_TRIVIAL(trace) << *this << " received echo reply";
}

void OpenflowConnection::handle_experimenter(
		fluid_msg::of13::Experimenter& experimenter_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received experimenter";

	// Send an error message
	fluid_msg::of13::Error error_msg(
		experimenter_message.xid(),
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_EXPERIMENTER);
	send_message( error_msg );
}

std::ostream& operator<<(std::ostream& os, const OpenflowConnection& con) {
	con.print_to_stream(os);
	return os;
}
