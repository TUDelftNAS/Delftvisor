#include "physical_switch.hpp"

#include <boost/log/trivial.hpp>

void PhysicalSwitch::handle_features_request(fluid_msg::of13::FeaturesRequest& features_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received features_request it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		features_request_message);
}

void PhysicalSwitch::handle_config_request(fluid_msg::of13::GetConfigRequest& config_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_config_request it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		config_request_message);
}
void PhysicalSwitch::handle_set_config(fluid_msg::of13::SetConfig& set_config_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received set_config it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		set_config_message);
}

void PhysicalSwitch::handle_barrier_request(fluid_msg::of13::BarrierRequest& barrier_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received barrier_request it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		barrier_request_message);
}

void PhysicalSwitch::handle_packet_out(fluid_msg::of13::PacketOut& packet_out_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received packet_out it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		packet_out_message);
}

void PhysicalSwitch::handle_flow_mod(fluid_msg::of13::FlowMod& flow_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received flow_mod it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		flow_mod_message);
}
void PhysicalSwitch::handle_group_mod(fluid_msg::of13::GroupMod& group_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received group_mod it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		group_mod_message);
}
void PhysicalSwitch::handle_port_mod(fluid_msg::of13::PortMod& port_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received port_mod it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		port_mod_message);
}
void PhysicalSwitch::handle_table_mod(fluid_msg::of13::TableMod& table_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received table_mod it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		table_mod_message);
}
void PhysicalSwitch::handle_meter_mod(fluid_msg::of13::MeterMod& meter_mod_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received meter_mod it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		meter_mod_message);
}

void PhysicalSwitch::handle_queue_config_request(fluid_msg::of13::QueueGetConfigRequest& queue_config_request) {
	BOOST_LOG_TRIVIAL(error) << *this << " received queue_get_config_request it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		queue_config_request);
}
void PhysicalSwitch::handle_queue_config_reply(fluid_msg::of13::QueueGetConfigReply& queue_config_reply) {
	BOOST_LOG_TRIVIAL(error) << *this << " received queue_get_config_reply it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		queue_config_reply);
}

void PhysicalSwitch::handle_role_request(fluid_msg::of13::RoleRequest& role_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received role_request it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		role_request_message);
}
void PhysicalSwitch::handle_role_reply(fluid_msg::of13::RoleReply& role_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received role_reply it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		role_reply_message);
}

void PhysicalSwitch::handle_get_async_request(fluid_msg::of13::GetAsyncRequest& async_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_async_request it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		async_request_message);
}
void PhysicalSwitch::handle_get_async_reply(fluid_msg::of13::GetAsyncReply& async_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_async_reply it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		async_reply_message);
}
void PhysicalSwitch::handle_set_async(fluid_msg::of13::SetAsync& set_async_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received set_async it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_TYPE,
		set_async_message);
}

void PhysicalSwitch::handle_multipart_request_desc(fluid_msg::of13::MultipartRequestDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request desc it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_flow(fluid_msg::of13::MultipartRequestFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request flow it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_aggregate(fluid_msg::of13::MultipartRequestAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request aggregate it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_table(fluid_msg::of13::MultipartRequestTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request table it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_port_stats(fluid_msg::of13::MultipartRequestPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request port stats it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_queue(fluid_msg::of13::MultipartRequestQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request queue it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_group(fluid_msg::of13::MultipartRequestGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request group it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_group_desc(fluid_msg::of13::MultipartRequestGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request group desc it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_group_features(fluid_msg::of13::MultipartRequestGroupFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request group features it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_meter(fluid_msg::of13::MultipartRequestMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request meter it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_meter_config(fluid_msg::of13::MultipartRequestMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request meter config it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_meter_features(fluid_msg::of13::MultipartRequestMeterFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request meter features it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_table_features(fluid_msg::of13::MultipartRequestTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request table features it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_port_desc(fluid_msg::of13::MultipartRequestPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request port description it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
void PhysicalSwitch::handle_multipart_request_experimenter(fluid_msg::of13::MultipartRequestExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request experimenter it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_EXPERIMENTER,
		multipart_request_message);
}

void PhysicalSwitch::handle_multipart_reply_desc(fluid_msg::of13::MultipartReplyDesc& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply desc it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_flow(fluid_msg::of13::MultipartReplyFlow& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply flow it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_aggregate(fluid_msg::of13::MultipartReplyAggregate& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply aggregate it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_table(fluid_msg::of13::MultipartReplyTable& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply table it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_port_stats(fluid_msg::of13::MultipartReplyPortStats& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply ports stats it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_queue(fluid_msg::of13::MultipartReplyQueue& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply queue it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_group(fluid_msg::of13::MultipartReplyGroup& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply group it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_group_desc(fluid_msg::of13::MultipartReplyGroupDesc& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply group desc it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_meter(fluid_msg::of13::MultipartReplyMeter& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply meter it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_meter_config(fluid_msg::of13::MultipartReplyMeterConfig& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply meter config it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_table_features(fluid_msg::of13::MultipartReplyTableFeatures& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply table features it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_reply_message);
}
void PhysicalSwitch::handle_multipart_reply_experimenter(fluid_msg::of13::MultipartReplyExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply experimenter  it shouldn't";

	// Send an error explaining this message is unsupported
	send_error_response(
		fluid_msg::of13::OFPET_BAD_REQUEST,
		fluid_msg::of13::OFPBRC_BAD_MULTIPART,
		multipart_request_message);
}
