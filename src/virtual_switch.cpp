#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>

#include "slice.hpp"
#include "hypervisor.hpp"
#include "virtual_switch.hpp"
#include "physical_switch.hpp"

VirtualSwitch::VirtualSwitch(
		boost::asio::io_service& io,
		uint64_t datapath_id,
		Hypervisor* hypervisor,
		Slice *slice)
	:
		OpenflowConnection::OpenflowConnection(io),
		datapath_id(datapath_id),
		hypervisor(hypervisor),
		slice(slice),
		state(down) {
}

void VirtualSwitch::add_port(
		uint32_t port_number,
		uint64_t physical_datapath_id,
		uint32_t physical_port_id) {
	ports[port_number].datapath_id = physical_datapath_id;
	ports[port_number].port_number = physical_port_id;
}

void VirtualSwitch::remove_port(uint32_t port_number) {
	ports.erase(port_number);
}

void VirtualSwitch::try_connect() {
	socket.async_connect(
		slice->get_controller_endpoint(),
		boost::bind(
			&VirtualSwitch::handle_connect,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void VirtualSwitch::handle_connect(const boost::system::error_code& error) {
	if( !error ) {
		OpenflowConnection::start();

		state = connected;

		BOOST_LOG_TRIVIAL(info) << *this << " got connected";

		// Send PortStatus messages for each port
		for( auto& port : ports ) {
			// TODO
		}
	}
	else {
		// Try connecting again?
		try_connect();
	}
}

void VirtualSwitch::start() {
	if( state==down ) {
		BOOST_LOG_TRIVIAL(info) << *this << " started";
		state = try_connecting;
		try_connect();
	}
}

void VirtualSwitch::stop() {
	if( state!=down ) {
		BOOST_LOG_TRIVIAL(info) << *this << " stopped";
		OpenflowConnection::stop();
		// socket.cancel()
		socket.close();

		state = down;
	}
}

bool VirtualSwitch::is_connected() {
	return state==connected;
}

void VirtualSwitch::check_online() {
	// If the slice hasn't been started don't do anything
	if( !slice->is_started() ) return;

	bool all_online_and_reachable = true;
	PhysicalSwitch::pointer first_switch = nullptr;

	for( auto& port : ports ) {
		// Lookup the PhysicalSwitch that owns this port
		auto switch_ptr = hypervisor->get_physical_switch_by_datapath_id(port.second.datapath_id);

		// TODO Make sure the physical switch actually has the
		// port that is referred to

		// and make sure that switch is online
		if( switch_ptr == nullptr ) {
			all_online_and_reachable = false;
			break;
		}

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
		start();
	}
	else if( !all_online_and_reachable && state!=down ) {
		stop();
	}
}

VirtualSwitch::pointer VirtualSwitch::shared_from_this() {
	return boost::static_pointer_cast<VirtualSwitch>(
			OpenflowConnection::shared_from_this());
}

void VirtualSwitch::handle_error(fluid_msg::of13::Error& error_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_features_request(fluid_msg::of13::FeaturesRequest& features_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_features_reply(fluid_msg::of13::FeaturesReply& features_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received features_reply it shouldn't";
}

void VirtualSwitch::handle_config_request(fluid_msg::of13::GetConfigRequest& config_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_config_reply(fluid_msg::of13::GetConfigReply& config_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_config_reply it shouldn't";
}
void VirtualSwitch::handle_set_config(fluid_msg::of13::SetConfig& set_config_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}

void VirtualSwitch::handle_barrier_request(fluid_msg::of13::BarrierRequest& barrier_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_barrier_reply(fluid_msg::of13::BarrierReply& barrier_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received barrier_reply it shouldn't";
}

void VirtualSwitch::handle_packet_in(fluid_msg::of13::PacketIn& packet_in_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received packet_in it shouldn't";
}
void VirtualSwitch::handle_packet_out(fluid_msg::of13::PacketOut& packet_out_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}

void VirtualSwitch::handle_flow_removed(fluid_msg::of13::FlowRemoved& flow_removed_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received flow_removed it shouldn't";
}
void VirtualSwitch::handle_port_status(fluid_msg::of13::PortStatus& port_status_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received port_status it shouldn't";
}

void VirtualSwitch::handle_flow_mod(fluid_msg::of13::FlowMod& flow_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_group_mod(fluid_msg::of13::GroupMod& group_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_port_mod(fluid_msg::of13::PortMod& port_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_table_mod(fluid_msg::of13::TableMod& table_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_meter_mod(fluid_msg::of13::MeterMod& meter_mod_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}

void VirtualSwitch::handle_queue_config_request(fluid_msg::of13::QueueGetConfigRequest& queue_config_request) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_queue_config_reply(fluid_msg::of13::QueueGetConfigReply& queue_config_reply) {
	BOOST_LOG_TRIVIAL(error) << *this << " received queue_get_config_reply it shouldn't";
}

void VirtualSwitch::handle_role_request(fluid_msg::of13::RoleRequest& role_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_role_reply(fluid_msg::of13::RoleReply& role_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received role_reply it shouldn't";
}

void VirtualSwitch::handle_get_async_request(fluid_msg::of13::GetAsyncRequest& async_request_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}
void VirtualSwitch::handle_get_async_reply(fluid_msg::of13::GetAsyncReply& async_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_async_reply it shouldn't";
}
void VirtualSwitch::handle_set_async(fluid_msg::of13::SetAsync& set_async_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error";
	// TODO
}

void VirtualSwitch::handle_multipart_request_desc(fluid_msg::of13::MultipartRequestDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_flow(fluid_msg::of13::MultipartRequestFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_aggregate(fluid_msg::of13::MultipartRequestAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_table(fluid_msg::of13::MultipartRequestTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_port_stats(fluid_msg::of13::MultipartRequestPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_queue(fluid_msg::of13::MultipartRequestQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_group(fluid_msg::of13::MultipartRequestGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_group_desc(fluid_msg::of13::MultipartRequestGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_group_features(fluid_msg::of13::MultipartRequestGroupFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_meter(fluid_msg::of13::MultipartRequestMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_meter_config(fluid_msg::of13::MultipartRequestMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_meter_features(fluid_msg::of13::MultipartRequestMeterFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_table_features(fluid_msg::of13::MultipartRequestTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_port_desc(fluid_msg::of13::MultipartRequestPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_request_experimenter(fluid_msg::of13::MultipartRequestExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_desc(fluid_msg::of13::MultipartReplyDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_flow(fluid_msg::of13::MultipartReplyFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_aggregate(fluid_msg::of13::MultipartReplyAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_table(fluid_msg::of13::MultipartReplyTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_port_stats(fluid_msg::of13::MultipartReplyPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_queue(fluid_msg::of13::MultipartReplyQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_group(fluid_msg::of13::MultipartReplyGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_group_desc(fluid_msg::of13::MultipartReplyGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_group_features(fluid_msg::of13::MultipartReplyGroupFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_meter(fluid_msg::of13::MultipartReplyMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_meter_config(fluid_msg::of13::MultipartReplyMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_meter_features(fluid_msg::of13::MultipartReplyMeterFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_table_features(fluid_msg::of13::MultipartReplyTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_port_desc(fluid_msg::of13::MultipartReplyPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_experimenter(fluid_msg::of13::MultipartReplyExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}

void VirtualSwitch::print_to_stream(std::ostream& os) const {
	os << "[Virtual switch dpid=" << datapath_id << ", online=" << (socket.is_open()) << "]";
}
