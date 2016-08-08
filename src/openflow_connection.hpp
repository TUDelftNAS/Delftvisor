#pragma once

#include <vector>
#include <queue>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread.hpp>

#include <fluid/of13msg.hh>

class OpenflowConnection : public boost::enable_shared_from_this<OpenflowConnection> {
private:
	/// The vector that stores new messages
	std::vector<uint8_t> message_buffer;
	/// Setup the wait to receive a message
	void start_receive_message();
	/// Receive the header of the openflow message
	void receive_header( const boost::system::error_code& error, std::size_t bytes_transferred );
	/// Receive the body of the openflow message
	void receive_body(const boost::system::error_code& error, std::size_t bytes_transferred);

	// TODO Look at http://www.boost.org/doc/libs/1_58_0/doc/html/atomic/usage_examples.html#boost_atomic.usage_examples.mp_queue , it does multi-producer 1 consumer queue without locks
	/// The mutex that protects the message queue
	boost::mutex send_queue_mutex;
	/// The queue of messages that need to be send
	/**
	 * The element at the front is the message currently
	 * being send.
	 */
	std::queue<std::vector<uint8_t>> send_queue;
	/// Send a message over this connection
	void send_message_queue_head();
	/// Handle a send message
	void handle_send_message(const boost::system::error_code& error, std::size_t bytes_transferred);

	/// A boolean to check if the echo request was answered
	bool echo_received;
	/// The timer that expires when an echo is due
	boost::asio::deadline_timer echo_timer;
	/// Schedule sending an echo message
	void schedule_echo_message();
	/// Send an echo request over this connection
	void send_echo_message();

protected:
	/// The boost socket object
	boost::asio::ip::tcp::socket socket;

	/// Send an openflow message over this connection
	// TODO Change type to libfluid message
	void send_message(fluid_msg::OFMsg* message);

	/// Handle a message that has been received
	virtual void handle_message() = 0;

	uint32_t get_next_xid();

public:
	/// Construct a new openflow connection
	OpenflowConnection(boost::asio::io_service& io);
	/// Get the socket of this connection
	boost::asio::ip::tcp::socket& get_socket();

	/// Start receiving and pinging this connection
	void start();
	/// Stop receiving and pinging this connection
	void stop();
};
