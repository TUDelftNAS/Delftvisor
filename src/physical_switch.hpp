#pragma once

#include <vector>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "openflow_connection.hpp"

class VirtualSwitch;
class Hypervisor;

/// Represents a port on this switch as it is in the network below
struct PhysicalPort {
	/// The virtual switches that depend on this port
	std::vector<boost::weak_ptr<VirtualSwitch>> dependent_virtual_switches;
	/// If we already offically have been told about this port
	bool found;
	/// The data concerning this port
	fluid_msg::of13::Port port_data;
};

class PhysicalSwitch : public OpenflowConnection {
private:
	/// The internal id used for routing
	int id;

	/// The hypervisor this physical switch belongs to
	Hypervisor* hypervisor;

	/// The current state of this switch
	enum {
		unregistered,
		registered
	} state;

	struct {
		/// The data in a features message
		int64_t datapath_id;
		uint32_t n_buffers;
		uint8_t n_tables;
		uint32_t capabilities;
		/// The data in a get_config message
		uint16_t flags;
		uint16_t miss_send_len;
	} features;

	/// The ports attached to this switch, port_id -> port
	std::unordered_map<uint32_t,PhysicalPort> ports;

	/// The timer that when fired sends a topology discovery packet
	boost::asio::deadline_timer topology_discovery_timer;

	/// The next port to send a topology discovery message over
	int topology_discovery_port;
	/// Schedule sending a topology discovery message
	void schedule_topology_discovery_message();
	/// Send the next topology discovery message
	void send_topology_discovery_message();

public:
	/// The constructor
	PhysicalSwitch(
			boost::asio::ip::tcp::socket& socket,
			int id,
			Hypervisor* hypervisor);

	typedef boost::shared_ptr<PhysicalSwitch> pointer;

	/// Allow creating a shared pointer of this class
	pointer shared_from_this();

	/// Start the switch, this means the socket is ready
	void start();
	/// Stop this switch
	void stop();

	void handle_error(fluid_msg::of13::Error& error_message);
	void handle_features_request(fluid_msg::of13::FeaturesRequest& features_request_message);
	void handle_features_reply  (fluid_msg::of13::FeaturesReply& features_reply_message);

	void handle_config_request(fluid_msg::of13::GetConfigRequest& config_request_message);
	void handle_config_reply  (fluid_msg::of13::GetConfigReply& config_reply_message);
	void handle_set_config    (fluid_msg::of13::SetConfig& set_config_message);

	void handle_barrier_request(fluid_msg::of13::BarrierRequest& barrier_request_message);
	void handle_barrier_reply  (fluid_msg::of13::BarrierReply& barrier_reply_message);

	void handle_packet_in (fluid_msg::of13::PacketIn& packet_in_message);
	void handle_packet_out(fluid_msg::of13::PacketOut& packet_out_message);

	void handle_flow_removed(fluid_msg::of13::FlowRemoved& flow_removed_message);
	void handle_port_status(fluid_msg::of13::PortStatus& port_status_message);

	void handle_flow_mod (fluid_msg::of13::FlowMod& flow_mod_message);
	void handle_group_mod(fluid_msg::of13::GroupMod& group_mod_message);
	void handle_port_mod (fluid_msg::of13::PortMod& port_mod_message);
	void handle_table_mod(fluid_msg::of13::TableMod& table_mod_message);
	void handle_meter_mod(fluid_msg::of13::MeterMod& meter_mod_message);

	void handle_multipart_request(fluid_msg::of13::MultipartRequest& multipart_request_message);
	void handle_multipart_reply  (fluid_msg::of13::MultipartReply& multipart_reply_message);

	void handle_queue_config_request(fluid_msg::of13::QueueGetConfigRequest& queue_config_request);
	void handle_queue_config_reply  (fluid_msg::of13::QueueGetConfigReply& queue_config_reply);

	void handle_role_request(fluid_msg::of13::RoleRequest& role_request_message);
	void handle_role_reply  (fluid_msg::of13::RoleReply& role_reply_message);

	void handle_get_async_request(fluid_msg::of13::GetAsyncRequest& async_request_message);
	void handle_get_async_reply  (fluid_msg::of13::GetAsyncReply& async_reply_message);
	void handle_set_async        (fluid_msg::of13::SetAsync& set_async_message);

	/// Print this physical switch to a stream
	void print_to_stream(std::ostream& os) const;
};
