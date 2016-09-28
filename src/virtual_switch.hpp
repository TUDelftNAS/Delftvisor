#pragma once

#include <map>
#include <unordered_map>

#include <boost/asio.hpp>

#include "bidirectional_map.hpp"

#include "openflow_connection.hpp"

class PhysicalSwitch;
class Hypervisor;
class Slice;

class VirtualSwitch : public OpenflowConnection {
private:
	/// The global id for this virtual switch
	int id;

	/// The datpath id of this switch
	uint64_t datapath_id;

	/// The hypervisor this virtual switch belongs to
	Hypervisor * hypervisor;

	/// The slice this virtual switch belongs to
	Slice* slice;

	/// The current state of this switch connection
	enum {
		down,
		try_connecting,
		connected
	} state;

	struct DependentSwitch {
		/// The mapping virtual port id <-> physical port id
		bidirectional_map<uint32_t,uint32_t> port_map;
	};
	/// The map with all the port id's
	/**
	 * physical_dpid -> (virtual_port_no -> physical_port_no)
	 */
	std::unordered_map<
		uint64_t,
		DependentSwitch> dependent_switches;
	/// The physical dpid that contains a virtual port
	/**
	 * virtual_port_no -> physical_dpid
	 */
	std::map<uint32_t,uint64_t> port_to_dependent_switch;

	/// The timer used to backoff between connection attempts
	boost::asio::deadline_timer connection_backoff_timer;
	/// The function called when the timer expires
	void backoff_expired(const boost::system::error_code& error);
	/// Try to connect to the controller
	void try_connect();
	/// The callback when the connection succeeds
	void handle_connect(const boost::system::error_code& error);

	/// Start this virtual switch, try to connect to the controller
	void start();
	/// Stop the controller connection of this virtual switch
	void stop();
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

	/// Get the unique id of this virtual switch
	int get_id() const;
	/// Get the slice this virtual switch is in
	const Slice* get_slice() const;

	/// Add a port to this virtual switch
	void add_port(
		uint32_t port_number,
		uint64_t physical_datapath_id,
		uint32_t physical_port_number);
	/// Remove a port from this virtual switch
	void remove_port(uint32_t port_number);
	/// Get the virtual -> physical port map for a dependent switch
	const bidirectional_map<uint32_t,uint32_t>& get_port_map(
		uint64_t physical_datapath_id) const;

	/// Check if this switch should be started/stopped
	/**
	 * This function is called after the topology of the physical
	 * network has changed. It checks if all physical switches this
	 * virtual switch depends on are online and if packets can be
	 * routed between all of them.
	 */
	void check_online();

	/// Tell this virtual switch to go down
	void go_down();
	/// Returns if this switch is currently connected
	bool is_connected();

	/// Print this virtual switch to a stream
	void print_to_stream(std::ostream& os) const;

	/// Handle openflow messages
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

	void handle_queue_config_request(fluid_msg::of13::QueueGetConfigRequest& queue_config_request);
	void handle_queue_config_reply  (fluid_msg::of13::QueueGetConfigReply& queue_config_reply);

	void handle_role_request(fluid_msg::of13::RoleRequest& role_request_message);
	void handle_role_reply  (fluid_msg::of13::RoleReply& role_reply_message);

	void handle_get_async_request(fluid_msg::of13::GetAsyncRequest& async_request_message);
	void handle_get_async_reply  (fluid_msg::of13::GetAsyncReply& async_reply_message);
	void handle_set_async        (fluid_msg::of13::SetAsync& set_async_message);

	void handle_multipart_request_desc          (fluid_msg::of13::MultipartRequestDesc& multipart_request_message);
	void handle_multipart_request_flow          (fluid_msg::of13::MultipartRequestFlow& multipart_request_message);
	void handle_multipart_request_aggregate     (fluid_msg::of13::MultipartRequestAggregate& multipart_request_message);
	void handle_multipart_request_table         (fluid_msg::of13::MultipartRequestTable& multipart_request_message);
	void handle_multipart_request_port_stats    (fluid_msg::of13::MultipartRequestPortStats& multipart_request_message);
	void handle_multipart_request_queue         (fluid_msg::of13::MultipartRequestQueue& multipart_request_message);
	void handle_multipart_request_group         (fluid_msg::of13::MultipartRequestGroup& multipart_request_message);
	void handle_multipart_request_group_desc    (fluid_msg::of13::MultipartRequestGroupDesc& multipart_request_message);
	void handle_multipart_request_group_features(fluid_msg::of13::MultipartRequestGroupFeatures& multipart_request_message);
	void handle_multipart_request_meter         (fluid_msg::of13::MultipartRequestMeter& multipart_request_message);
	void handle_multipart_request_meter_config  (fluid_msg::of13::MultipartRequestMeterConfig& multipart_request_message);
	void handle_multipart_request_meter_features(fluid_msg::of13::MultipartRequestMeterFeatures& multipart_request_message);
	void handle_multipart_request_table_features(fluid_msg::of13::MultipartRequestTableFeatures& multipart_request_message);
	void handle_multipart_request_port_desc     (fluid_msg::of13::MultipartRequestPortDescription& multipart_request_message);
	void handle_multipart_request_experimenter  (fluid_msg::of13::MultipartRequestExperimenter& multipart_request_message);
	void handle_multipart_reply_desc          (fluid_msg::of13::MultipartReplyDesc& multipart_request_message);
	void handle_multipart_reply_flow          (fluid_msg::of13::MultipartReplyFlow& multipart_request_message);
	void handle_multipart_reply_aggregate     (fluid_msg::of13::MultipartReplyAggregate& multipart_request_message);
	void handle_multipart_reply_table         (fluid_msg::of13::MultipartReplyTable& multipart_request_message);
	void handle_multipart_reply_port_stats    (fluid_msg::of13::MultipartReplyPortStats& multipart_request_message);
	void handle_multipart_reply_queue         (fluid_msg::of13::MultipartReplyQueue& multipart_request_message);
	void handle_multipart_reply_group         (fluid_msg::of13::MultipartReplyGroup& multipart_request_message);
	void handle_multipart_reply_group_desc    (fluid_msg::of13::MultipartReplyGroupDesc& multipart_request_message);
	void handle_multipart_reply_group_features(fluid_msg::of13::MultipartReplyGroupFeatures& multipart_request_message);
	void handle_multipart_reply_meter         (fluid_msg::of13::MultipartReplyMeter& multipart_request_message);
	void handle_multipart_reply_meter_config  (fluid_msg::of13::MultipartReplyMeterConfig& multipart_request_message);
	void handle_multipart_reply_meter_features(fluid_msg::of13::MultipartReplyMeterFeatures& multipart_request_message);
	void handle_multipart_reply_table_features(fluid_msg::of13::MultipartReplyTableFeatures& multipart_request_message);
	void handle_multipart_reply_port_desc     (fluid_msg::of13::MultipartReplyPortDescription& multipart_request_message);
	void handle_multipart_reply_experimenter  (fluid_msg::of13::MultipartReplyExperimenter& multipart_request_message);
};
