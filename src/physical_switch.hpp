#pragma once

#include <set>
#include <unordered_set>
#include <unordered_map>

#include <boost/asio.hpp>

#include "id_allocator.hpp"
#include "bidirectional_map.hpp"

#include "openflow_connection.hpp"

class DiscoveredLink;
class VirtualSwitch;
class Hypervisor;

namespace topology {
	/// The value used for infinite for floyd-warshall, this value should
	/// be choosen such that it doesn't overflow when it get's added to
	/// itself but is also longer than the longest possible path in the
	/// network.
	constexpr int infinite = 10000;
	constexpr int period   = 1000; // The period to send all topology messages in in ms
}

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

	struct Features {
		/// The data in a features message
		uint64_t datapath_id;
		uint32_t n_buffers;
		uint8_t n_tables;
		uint32_t capabilities;
		/// The data in a get_config message
		uint16_t flags;
		uint16_t miss_send_len;
	} features;

	/// The meter features
	fluid_msg::of13::MeterFeatures meter_features;
	/// The group features
	fluid_msg::of13::GroupFeatures group_features;

	/// The information needed when forwarding a response
	struct RequestSource {
		uint32_t original_xid;
		boost::weak_ptr<VirtualSwitch> virtual_switch;
	};
	/// The xid translator
	std::unordered_map<
		uint32_t,
		RequestSource> xid_map;
	/// Send a message that needs a response
	/**
	 * This version stores the original xid this message was
	 * send with so the response can be forwarded to the appropriate
	 * virtual switch.
	 */
	void send_request_message(
		fluid_msg::OFMsg& message,
		boost::weak_ptr<VirtualSwitch> virtual_switch);

	/// Represents a port on this switch as it is in the network below
	struct Port {
		/// The internal id for this port
		//int id;
		/// Save per port what rule in the first flow table is pushed
		enum State {
			no_rule,
			link_rule,
			host_rule,
			drop_rule
		} state;
		/// Translate a port state to a string for debugging
		static const std::string state_to_string(State state);
		/// If this port has a link to another switch
		boost::shared_ptr<DiscoveredLink> link;
		/// The data concerning this port
		fluid_msg::of13::Port port_data;
	};
	/// The ports attached to this switch, port_id -> port
	std::unordered_map<
		uint32_t,
		Port> ports;

	struct NeededPort {
		bool rule_installed;
		boost::shared_ptr<VirtualSwitch> virtual_switch;
	};
	/// The ports that are searched for on this switch, port_id -> set<VirtualSwitch*>
	/**
	 * This structure is separate from ports since not all
	 * ports that are needed have to already have been registered.
	 */
	std::unordered_map<
		uint32_t,
		// A map from virtual switch id -> NeededPort
		std::unordered_map<uint32_t,NeededPort>> needed_ports;

	/// Handle information about a port we received
	/**
	 * Two messages contain port information, the PortStatus
	 * message and the MultipartReplyPortDescription. All the
	 * port data provided gets handled by this message which
	 * updates the ports information and updates the virtual
	 * switches if necessary.
	 */
	void handle_port(fluid_msg::of13::Port& port, uint8_t reason);

	/// Allocate valid group id's
	/**
	 * The group with id 0 is reserved to output to the controller.
	 */
	IdAllocator<1,UINT32_MAX> group_id_allocator;
	/// A group created to be used as output port in a virtual switch
	struct OutputGroup {
		/// The group id of this OutputGroup
		uint32_t group_id;
		/// The state of the action in this group
		enum State {
			no_rule,
			host_rule,
			shared_link_rule,
			switch_rule
		} state;
		/// Translate an output group state to a string
		static const std::string state_to_string(State state);
		/// The physical port this rule currently outputs over
		uint32_t output_port;
	};
	/// An entry with the rewrite information for 1 virtual switch
	struct RewriteEntry {
		/// The group id for the flood action
		uint32_t flood_group_id;
		/// A bidirectional mapping between virtual group id <-> physical group id
		bidirectional_map<uint32_t,uint32_t> group_id_map;
		/// A map from (virtual port id) -> OutputGroup
		std::unordered_map<uint32_t,OutputGroup> output_groups;
	};
	/// A mapping from virtual switch id -> RewriteEntry
	/**
	 * This structure contains all information needed to
	 * rewrite id's between the physical and the virtual
	 * except for the port id's. For each virtual switch
	 * it also contains the information about the groups
	 * created.
	 */
	std::unordered_map<int, RewriteEntry> rewrite_map;


	/// The timer that when fired sends a topology discovery packet
	boost::asio::deadline_timer topology_discovery_timer;
	/// Create the flowrule in this switch to forward topology discovery messages
	void make_topology_discovery_rule();
	/// The next port to send a topology discovery message over
	int topology_discovery_port;
	/// Schedule sending a topology discovery message
	void schedule_topology_discovery_message();
	/// Send the next topology discovery message
	void send_topology_discovery_message(const boost::system::error_code& error);
	/// Handle a packet in for topology discovery
	void handle_topology_discovery_packet_in(
		fluid_msg::of13::PacketIn& packet_in_message);

	/// The distance from this switch to other switches (switch_id -> distance)
	std::unordered_map<int,int> dist;
	/// To what port to forward traffic to get to a switch (switch_id -> port_number)
	std::unordered_map<int,uint32_t> next;
	/// The currently set port to forward traffic to for each switch (switch id -> port number)
	std::unordered_map<int,uint32_t> current_next;

	/// Setup the flow table with the static initial rules
	void create_static_rules();

public:
	typedef boost::shared_ptr<PhysicalSwitch> pointer;

	/// The constructor
	PhysicalSwitch(
			boost::asio::ip::tcp::socket& socket,
			int id,
			Hypervisor* hypervisor);

	/// Get the internal id
	int get_id() const;

	/// Get the features of this switch
	const Features& get_features() const;
	/// Get the group features of this switch
	const fluid_msg::of13::GroupFeatures& get_group_features() const;
	/// Get the meter features of this switch
	const fluid_msg::of13::MeterFeatures& get_meter_features() const;

	/// Get the ports on this switch
	const std::unordered_map<uint32_t,Port>& get_ports() const;

	/// Register a virtual switch interest
	void register_interest(boost::shared_ptr<VirtualSwitch> virtual_switch);
	/// Remove a virtual switch interest
	void remove_interest(boost::shared_ptr<VirtualSwitch> virtual_switch);

	/// Allow creating a shared pointer of this class
	pointer shared_from_this();

	/// Start the switch, this means the socket is ready
	void start();
	/// Stop this switch
	void stop();

	/// Add a discover link to this physical switch
	void add_link(boost::shared_ptr<DiscoveredLink> discovered_link);
	/// Reset a link involving this switch
	void reset_link(boost::shared_ptr<DiscoveredLink> discovered_link);

	/// Reset all the floyd-warshall data to begin state
	void reset_distances();
	/// Get the known distance to a switch
	int get_distance(int switch_id);
	/// Set a new distance to a switch
	void set_distance(int switch_id, int distance);
	/// Get the port to forward traffic over to get to a switch
	uint32_t get_next(int switch_id);
	/// Set the port to forward traffic over to get to a switch
	void set_next(int switch_id, uint32_t port_number);

	/// Update the dynamic rules and groups after the topology has changed
	void update_dynamic_rules();

	/// Rewrite a group id for a specific virtual switch
	uint32_t get_rewritten_group_id(
		uint32_t virtual_group_id,
		const VirtualSwitch* virtual_switch);
	/// Rewrite an InstructionSet for this physical switch
	bool rewrite_instruction_set(
		fluid_msg::of13::InstructionSet& old_instruction_set,
		fluid_msg::of13::InstructionSet& instruction_set_with_output,
		fluid_msg::of13::InstructionSet& instruction_set_without_output,
		bool& has_action_with_group,
		const VirtualSwitch* virtual_switch);
	/// Rewrite an action set for this physical switch
	bool rewrite_action_set(
		fluid_msg::ActionSet& old_action_set,
		fluid_msg::ActionSet& action_set_with_output,
		fluid_msg::ActionSet& action_set_without_output,
		bool& has_action_with_group,
		const VirtualSwitch* virtual_switch);
	/// Rewrite an action list for this physical switch
	bool rewrite_action_list(
		fluid_msg::ActionList& old_action_list,
		fluid_msg::ActionList& new_action_list,
		const VirtualSwitch* virtual_switch);
	/// Rewrite the match of an flowmod
	bool rewrite_match(
		fluid_msg::of13::Match& match,
		const VirtualSwitch* virtual_switch);

	/// The message handling functions
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

	void handle_queue_config_request(fluid_msg::of13::QueueGetConfigRequest& queue_config_request);
	void handle_queue_config_reply  (fluid_msg::of13::QueueGetConfigReply& queue_config_reply);

	void handle_role_request(fluid_msg::of13::RoleRequest& role_request_message);
	void handle_role_reply  (fluid_msg::of13::RoleReply& role_reply_message);

	void handle_get_async_request(fluid_msg::of13::GetAsyncRequest& async_request_message);
	void handle_get_async_reply  (fluid_msg::of13::GetAsyncReply& async_reply_message);
	void handle_set_async        (fluid_msg::of13::SetAsync& set_async_message);

	/// Print a quick identifyable name for this physical switch
	void print_to_stream(std::ostream& os) const;
	/// Print almost complete debugging info about this physical switch
	void print_detailed(std::ostream& os) const;

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
