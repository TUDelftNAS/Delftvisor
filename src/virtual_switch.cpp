#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>

#include "slice.hpp"
#include "hypervisor.hpp"
#include "virtual_switch.hpp"
#include "physical_switch.hpp"
#include "slice.hpp"

VirtualSwitch::VirtualSwitch(
		boost::asio::io_service& io,
		uint64_t datapath_id,
		Hypervisor* hypervisor,
		Slice *slice)
	:
		OpenflowConnection::OpenflowConnection(io),
		connection_backoff_timer(io),
		datapath_id(datapath_id),
		hypervisor(hypervisor),
		slice(slice),
		state(down) {
}

void VirtualSwitch::add_port(
		uint32_t port_number,
		uint64_t physical_datapath_id,
		uint32_t physical_port_number) {
	// Store the lookup link
	port_to_dependent_switch
		[physical_datapath_id] = port_number;
	dependent_switches
		[physical_datapath_id]
		[port_number] = physical_port_number;
}

void VirtualSwitch::remove_port(uint32_t port_number) {
	// Remove from the port_to_dependent_switch structure
	uint64_t physical_dpid = port_to_dependent_switch.at(port_number);
	port_to_dependent_switch.erase(port_number);

	// Remove from dependent_switches
	dependent_switches.at(physical_dpid).erase(port_number);
	if( dependent_switches.at(physical_dpid).size() == 0 ) {
		dependent_switches.erase(physical_dpid);
	}
}

void VirtualSwitch::try_connect() {
	state = try_connecting;
	socket.async_connect(
		slice->get_controller_endpoint(),
		boost::bind(
			&VirtualSwitch::handle_connect,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void VirtualSwitch::handle_connect(const boost::system::error_code& error) {
	if( !error ) {
		start();
	}
	else {
		// Try connecting again?
		if( state==try_connecting ) {
			connection_backoff_timer.expires_from_now(
				boost::posix_time::milliseconds(500));
			connection_backoff_timer.async_wait(
				boost::bind(
					&VirtualSwitch::backoff_expired,
					shared_from_this(),
					boost::asio::placeholders::error));
		}
	}
}

void VirtualSwitch::backoff_expired(
		const boost::system::error_code& error) {
	if( !error ) {
		BOOST_LOG_TRIVIAL(trace) << *this << " connecting failed, trying again";
		try_connect();
	}
	else {
		BOOST_LOG_TRIVIAL(error) << *this << " backoff error " << error.message();
	}
}

void VirtualSwitch::start() {
	if( state==try_connecting ) {
		OpenflowConnection::start();
		state = connected;
		BOOST_LOG_TRIVIAL(info) << *this << " started";
	}
}

void VirtualSwitch::stop() {
	// Clean up the openflow connection structures
	OpenflowConnection::stop();

	// Stop any work in the backoff timer
	connection_backoff_timer.cancel();

	// If we the connection was stopped by the controller
	// immediatly try again
	if( state==connected ) {
		BOOST_LOG_TRIVIAL(info) << *this <<
			" connection dropped, trying again";
		try_connect();
	}
	else {
		BOOST_LOG_TRIVIAL(error) << *this <<
			" cannot stop since not connected";
	}
}

void VirtualSwitch::go_down() {
	state = down;
	stop();
}

bool VirtualSwitch::is_connected() {
	return state==connected;
}

void VirtualSwitch::check_online() {
	// If the slice hasn't been started don't do anything
	if( !slice->is_started() ) return;

	bool all_online_and_reachable = true;
	PhysicalSwitch::pointer first_switch = nullptr;

	for( auto& dep_sw : dependent_switches ) {
		// Lookup the PhysicalSwitch via the hypervisor
		auto switch_ptr = hypervisor
			->get_physical_switch_by_datapath_id(
				dep_sw.first);

		// Make sure that switch is online
		if( switch_ptr == nullptr ) {
			all_online_and_reachable = false;
			break;
		}

		// TODO Make sure the physical switch actually has
		// all the ports that are asked of it

		if( first_switch == nullptr ) {
			// Set the current PhysicalSwitch in the first_switch
			// variable
			first_switch = switch_ptr;
		}
		else {
			// Check connectivity between the current PhysicalSwitch
			// and *first_switch
			if( first_switch->get_distance(switch_ptr->get_id()) == topology::infinite ) {
				all_online_and_reachable = false;
				break;
			}
		}
	}

	//BOOST_LOG_TRIVIAL(info) << *this << " checked for online, all_online_and_reachable=" << all_online_and_reachable << " state=" << state;

	// Update this virtual switch state if needed
	if( all_online_and_reachable && state==down ) {
		try_connect();
	}
	else if( !all_online_and_reachable && state!=down ) {
		go_down();
	}
}

VirtualSwitch::pointer VirtualSwitch::shared_from_this() {
	return boost::static_pointer_cast<VirtualSwitch>(
			OpenflowConnection::shared_from_this());
}

void VirtualSwitch::print_to_stream(std::ostream& os) const {
	os << "[Virtual switch dpid=" << datapath_id << ", state=" << (state==down?"down":(state==try_connecting?"try_connecting":"connected")) << "]";
}

void VirtualSwitch::handle_error(fluid_msg::of13::Error& error_message) {
	BOOST_LOG_TRIVIAL(error) << *this
		<< " received error Type=" << error_message.err_type()
		<< " Code=" << error_message.code();
}

void VirtualSwitch::handle_features_request(fluid_msg::of13::FeaturesRequest& features_request_message) {
	// Lookup the features of all switches below
	uint32_t n_buffers    = UINT32_MAX;
	uint8_t n_tables      = UINT8_MAX;
	uint32_t capabilities = UINT32_MAX;

	for( auto& dep_sw : dependent_switches ) {
		auto phy_sw = hypervisor
			->get_physical_switch_by_datapath_id(
				dep_sw.first);

		if( phy_sw == nullptr ) {
			BOOST_LOG_TRIVIAL(error) << *this <<
				" not all switches online?";
		}

		const auto& features = phy_sw->get_features();
		n_buffers = std::min( n_buffers, features.n_buffers );
		n_tables = std::min( n_tables, features.n_tables );
		capabilities &= features.capabilities;
	}

	// We reserve 2 tables for the hypervisor
	n_tables -= 2;
	// Statistics are not supported in this version of the hypervisor
	capabilities &= fluid_msg::of13::OFPC_IP_REASM | fluid_msg::of13::OFPC_PORT_BLOCKED;

	// Create the response message
	fluid_msg::of13::FeaturesReply features_reply(
		features_request_message.xid(),
		datapath_id,
		n_buffers,
		n_tables,
		0, // Auxiliary id
		capabilities);

	// Send the message response
	send_message_response(features_reply);

	BOOST_LOG_TRIVIAL(info) << *this << " received features_request";
}

void VirtualSwitch::handle_barrier_request(fluid_msg::of13::BarrierRequest& barrier_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received barrier_request";
	// TODO
}

void VirtualSwitch::handle_packet_out(fluid_msg::of13::PacketOut& packet_out_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received packet_out";
	// TODO
}

void VirtualSwitch::handle_flow_mod(fluid_msg::of13::FlowMod& flow_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received flow_mod";
	// TODO
}
void VirtualSwitch::handle_group_mod(fluid_msg::of13::GroupMod& group_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received group_mod";
	// TODO
}
void VirtualSwitch::handle_port_mod(fluid_msg::of13::PortMod& port_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received port_mod";
	// TODO
}
void VirtualSwitch::handle_table_mod(fluid_msg::of13::TableMod& table_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received table_mod";
	// TODO
}
void VirtualSwitch::handle_meter_mod(fluid_msg::of13::MeterMod& meter_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received meter_mod";
	// TODO
}

void VirtualSwitch::handle_multipart_request_port_desc(fluid_msg::of13::MultipartRequestPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received multipart_request_port_description";
	// TODO
}

