#pragma once

#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "openflow_connection.hpp"

class PhysicalSwitch;
class Hypervisor;
class Slice;

struct VirtualPort {
	uint64_t datapath_id;
	uint32_t port_number;
};

class VirtualSwitch : public OpenflowConnection {
private:
	/// The datpath id of this switch
	uint64_t datapath_id;

	/// The current state of this switch connection
	enum {
		down,
		try_connecting,
		connected
	} state;

	/// The virtual ports on this switch
	std::unordered_map<uint32_t,VirtualPort> ports;

	/// The hypervisor this virtual switch belongs to
	Hypervisor * hypervisor;

	/// The slice this virtual switch belongs to
	Slice* slice;

	/// Try to connect to the controller
	void try_connect();
	/// The callback when the connection succeeds
	void handle_connect( const boost::system::error_code& error );
public:
	typedef boost::shared_ptr<VirtualSwitch> pointer;

	/// Allow creating a shared pointer of this class
	pointer shared_from_this();

	/// Create a new virtual switch object
	VirtualSwitch(
		boost::asio::io_service& io,
		uint64_t datapath_id,
		Hypervisor* hypervisor,
		Slice* slice);

	void add_port(
		uint32_t port_number,
		uint64_t physical_datapath_id,
		uint32_t physical_port_id);
	void remove_port(uint32_t port_number);

	/// Check if this switch should be started/stopped
	/**
	 * This function is called after the topology of the physical
	 * network has changed. It checks if all physical switches this
	 * virtual switch depends on are online and if packets can be
	 * routed between all of them.
	 */
	void check_online();

	/// Start this virtual switch, try to connect to the controller
	void start();
	/// Stop the controller connection of this virtual switch
	void stop();
	/// Returns if this switch is currently connected
	bool is_connected();

	void handle_error           (fluid_msg::of13::Error& error_message);
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
	void handle_port_status (fluid_msg::of13::PortStatus& port_status_message);

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

	/// Print this virtual switch to a stream
	void print_to_stream(std::ostream& os) const;
};
