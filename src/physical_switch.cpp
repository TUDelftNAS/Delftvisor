#include "physical_switch.hpp"
#include "virtual_switch.hpp"
#include "hypervisor.hpp"

#include <boost/log/trivial.hpp>

PhysicalSwitch::PhysicalSwitch(
		boost::asio::ip::tcp::socket& socket,
		int id,
		Hypervisor* hypervisor)
	:
		OpenflowConnection::OpenflowConnection(socket),
		topology_discovery_timer(socket.get_io_service()),
		id(id),
		hypervisor(hypervisor),
		state(unregistered) {
	if( id >= 4096 ) {
		BOOST_LOG_TRIVIAL(fatal) << "Ran out of switch id's";
		// Crash or something?
	}
	// Set this one here already because the value is printed
	features.datapath_id = 0;
}

void PhysicalSwitch::start() {
	// Start up the generic connection handling
	OpenflowConnection::start();

	// Send an featuresrequest
	fluid_msg::of13::FeaturesRequest features_message(get_next_xid());
	send_message( features_message );

	BOOST_LOG_TRIVIAL(info) << *this << " started";
}

void PhysicalSwitch::stop() {
	// Stop the generic connection handling
	OpenflowConnection::stop();

	if( state == unregistered ) {
		hypervisor->unregister_physical_switch(id);
	}
	else {
		hypervisor->unregister_physical_switch(features.datapath_id,id);
	}

	BOOST_LOG_TRIVIAL(info) << *this << " stopped";
}

PhysicalSwitch::pointer PhysicalSwitch::shared_from_this() {
	return boost::static_pointer_cast<PhysicalSwitch>(
			OpenflowConnection::shared_from_this());
}

void PhysicalSwitch::handle_error(fluid_msg::of13::Error& error_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void PhysicalSwitch::handle_features_request(fluid_msg::of13::FeaturesRequest& features_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received features_request it shouldn't";
}
void PhysicalSwitch::handle_features_reply(fluid_msg::of13::FeaturesReply& features_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received features_reply";

	if( state == registered ) {
		BOOST_LOG_TRIVIAL(error) << *this << " received features_reply while already registered";
	}

	features.datapath_id  = features_reply_message.datapath_id();
	features.n_buffers    = features_reply_message.n_buffers();
	features.n_tables     = features_reply_message.n_tables();
	features.capabilities = features_reply_message.capabilities();

	hypervisor->register_physical_switch(features.datapath_id,id);
	state = registered;
}

void PhysicalSwitch::handle_config_request(fluid_msg::of13::GetConfigRequest& config_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_config_request it shouldn't";
}
void PhysicalSwitch::handle_config_reply(fluid_msg::of13::GetConfigReply& config_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received get_config_reply";

	features.flags         = config_reply_message.flags();
	features.miss_send_len = config_reply_message.flags();
}
void PhysicalSwitch::handle_set_config(fluid_msg::of13::SetConfig& set_config_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received set_config it shouldn't";
}

void PhysicalSwitch::handle_barrier_request(fluid_msg::of13::BarrierRequest& barrier_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received barrier_request it shouldn't";
}
void PhysicalSwitch::handle_barrier_reply(fluid_msg::of13::BarrierReply& barrier_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received barrier_reply";

	// TODO
	// Figure out who requested this
	// Mark this switch as done
	// If all physical switches are done
	//   send BarrierReply
}

void PhysicalSwitch::handle_packet_in(fluid_msg::of13::PacketIn& packet_in_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received packet_in";
	// TODO
}
void PhysicalSwitch::handle_packet_out(fluid_msg::of13::PacketOut& packet_out_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received packet_out it shouldn't";
}

void PhysicalSwitch::handle_flow_removed(fluid_msg::of13::FlowRemoved& flow_removed_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received flow_removed";
	// TODO
}
void PhysicalSwitch::handle_port_status(fluid_msg::of13::PortStatus& port_status_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received port_status";
	// Check if this port is already registered, if registered
	// propagate modify message otherwise add message. If it is
	// a delete message propagate that.

	uint32_t port_number = port_status_message.desc().port_no();
	auto search = ports.find(port_number);
	if( search == ports.end() ) {
		if( port_status_message.reason() == fluid_msg::of13::OFPPR_DELETE ) {
			// If this is a delete message for a port we didn't know about
			// don't do anything.
			return;
		}
		// This is a new port we didn't know about
		port_status_message.reason(fluid_msg::of13::OFPPR_ADD);
	}
	else {
		if( port_status_message.reason() == fluid_msg::of13::OFPPR_DELETE ) {
			if( ports[port_number].dependent_virtual_switches.size() == 0 ) {
				// Delete this port from the switch
				ports.erase(port_number);
				return;
			}
			else {
				// Mark this port down
				ports[port_number].found = false;
			}
		}
		else if(ports[port_number].found) {
			// Make this a modify message
			port_status_message.reason(fluid_msg::of13::OFPPR_MODIFY);
		}
	}

	// Loop over the depended switches and make them check again
	for( auto w_ptr : ports[port_number].dependent_virtual_switches ) {
		auto ptr = w_ptr.lock();
		// TODO Rewrite the port number
		ptr->send_message(port_status_message);
		ptr->check_online();
	}
}

void PhysicalSwitch::handle_flow_mod(fluid_msg::of13::FlowMod& flow_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received flow_mod it shouldn't";
}
void PhysicalSwitch::handle_group_mod(fluid_msg::of13::GroupMod& group_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received group_mod it shouldn't";
}
void PhysicalSwitch::handle_port_mod(fluid_msg::of13::PortMod& port_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received port_mod it shouldn't";
}
void PhysicalSwitch::handle_table_mod(fluid_msg::of13::TableMod& table_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received table_mod it shouldn't";
}
void PhysicalSwitch::handle_meter_mod(fluid_msg::of13::MeterMod& meter_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received meter_mod it shouldn't";
}

void PhysicalSwitch::handle_multipart_request(fluid_msg::of13::MultipartRequest& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart_request it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply(fluid_msg::of13::MultipartReply& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart_reply it shouldn't";
}

void PhysicalSwitch::handle_queue_config_request(fluid_msg::of13::QueueGetConfigRequest& queue_config_request) {
	BOOST_LOG_TRIVIAL(error) << *this << " received queue_get_config_request it shouldn't";
}
void PhysicalSwitch::handle_queue_config_reply(fluid_msg::of13::QueueGetConfigReply& queue_config_reply) {
	BOOST_LOG_TRIVIAL(error) << *this << " received queue_get_config_reply it shouldn't";
}

void PhysicalSwitch::handle_role_request(fluid_msg::of13::RoleRequest& role_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received role_request it shouldn't";
}
void PhysicalSwitch::handle_role_reply(fluid_msg::of13::RoleReply& role_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received role_reply it shouldn't";
}

void PhysicalSwitch::handle_get_async_request(fluid_msg::of13::GetAsyncRequest& async_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_async_request it shouldn't";
}
void PhysicalSwitch::handle_get_async_reply(fluid_msg::of13::GetAsyncReply& async_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_async_reply it shouldn't";
}
void PhysicalSwitch::handle_set_async(fluid_msg::of13::SetAsync& set_async_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received set_async it shouldn't";
}

void PhysicalSwitch::print_to_stream(std::ostream& os) const {
	os << "[PhysicalSwitch id=" << id << ", dpid=" << features.datapath_id << "]";
}
