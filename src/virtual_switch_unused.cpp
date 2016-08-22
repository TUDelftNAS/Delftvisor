#include "virtual_switch.hpp"

#include <boost/log/trivial.hpp>

void VirtualSwitch::handle_features_reply(fluid_msg::of13::FeaturesReply& features_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received features_reply it shouldn't";
}

void VirtualSwitch::handle_config_reply(fluid_msg::of13::GetConfigReply& config_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_config_reply it shouldn't";
}

void VirtualSwitch::handle_barrier_reply(fluid_msg::of13::BarrierReply& barrier_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received barrier_reply it shouldn't";
}

void VirtualSwitch::handle_packet_in(fluid_msg::of13::PacketIn& packet_in_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received packet_in it shouldn't";
}

void VirtualSwitch::handle_flow_removed(fluid_msg::of13::FlowRemoved& flow_removed_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received flow_removed it shouldn't";
}
void VirtualSwitch::handle_port_status(fluid_msg::of13::PortStatus& port_status_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received port_status it shouldn't";
}

void VirtualSwitch::handle_queue_config_reply(fluid_msg::of13::QueueGetConfigReply& queue_config_reply) {
	BOOST_LOG_TRIVIAL(error) << *this << " received queue_get_config_reply it shouldn't";
}

void VirtualSwitch::handle_role_reply(fluid_msg::of13::RoleReply& role_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received role_reply it shouldn't";
}

void VirtualSwitch::handle_get_async_reply(fluid_msg::of13::GetAsyncReply& async_reply_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received get_async_reply it shouldn't";
}

void VirtualSwitch::handle_multipart_request_desc(fluid_msg::of13::MultipartRequestDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request desc it shouldn't";
}
void VirtualSwitch::handle_multipart_request_flow(fluid_msg::of13::MultipartRequestFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request flow it shouldn't";
}
void VirtualSwitch::handle_multipart_request_aggregate(fluid_msg::of13::MultipartRequestAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request aggregate it shouldn't";
}
void VirtualSwitch::handle_multipart_request_table(fluid_msg::of13::MultipartRequestTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request table it shouldn't";
}
void VirtualSwitch::handle_multipart_request_port_stats(fluid_msg::of13::MultipartRequestPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request port stats it shouldn't";
}
void VirtualSwitch::handle_multipart_request_queue(fluid_msg::of13::MultipartRequestQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request queue it shouldn't";
}
void VirtualSwitch::handle_multipart_request_group(fluid_msg::of13::MultipartRequestGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request group it shouldn't";
}
void VirtualSwitch::handle_multipart_request_group_desc(fluid_msg::of13::MultipartRequestGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request group desc it shouldn't";
}
void VirtualSwitch::handle_multipart_request_group_features(fluid_msg::of13::MultipartRequestGroupFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request group features it shouldn't";
}
void VirtualSwitch::handle_multipart_request_meter(fluid_msg::of13::MultipartRequestMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request meter it shouldn't";
}
void VirtualSwitch::handle_multipart_request_meter_config(fluid_msg::of13::MultipartRequestMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request meter config it shouldn't";
}
void VirtualSwitch::handle_multipart_request_meter_features(fluid_msg::of13::MultipartRequestMeterFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request meter features it shouldn't";
}
void VirtualSwitch::handle_multipart_request_table_features(fluid_msg::of13::MultipartRequestTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request table features it shouldn't";
}
void VirtualSwitch::handle_multipart_request_port_desc(fluid_msg::of13::MultipartRequestPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request port description it shouldn't";
}
void VirtualSwitch::handle_multipart_request_experimenter(fluid_msg::of13::MultipartRequestExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request experimenter it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_desc(fluid_msg::of13::MultipartReplyDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply desc it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_flow(fluid_msg::of13::MultipartReplyFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply flow it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_aggregate(fluid_msg::of13::MultipartReplyAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply aggregate it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_table(fluid_msg::of13::MultipartReplyTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply table it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_port_stats(fluid_msg::of13::MultipartReplyPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply port stats it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_queue(fluid_msg::of13::MultipartReplyQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply queue it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_group(fluid_msg::of13::MultipartReplyGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply group it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_group_desc(fluid_msg::of13::MultipartReplyGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply group desc it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_group_features(fluid_msg::of13::MultipartReplyGroupFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply group features it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_meter(fluid_msg::of13::MultipartReplyMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply meter it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_meter_config(fluid_msg::of13::MultipartReplyMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply meter config it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_meter_features(fluid_msg::of13::MultipartReplyMeterFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply meter features it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_table_features(fluid_msg::of13::MultipartReplyTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply table features it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_port_desc(fluid_msg::of13::MultipartReplyPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply port description it shouldn't";
}
void VirtualSwitch::handle_multipart_reply_experimenter(fluid_msg::of13::MultipartReplyExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply experimenter it shouldn't";
}
