#include "physical_switch.hpp"
#include "virtual_switch.hpp"
#include "hypervisor.hpp"
#include "discoveredlink.hpp"

#include "vlan_tag.hpp"

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

int PhysicalSwitch::get_id() const {
	return id;
}

const std::unordered_map<uint32_t,PhysicalSwitch::PhysicalPort>& PhysicalSwitch::get_ports() const
{
	return ports;
}

void PhysicalSwitch::register_port_interest(
		uint32_t port,
		boost::shared_ptr<VirtualSwitch> switch_pointer) {
	needed_ports[port].insert(switch_pointer);
}
void PhysicalSwitch::remove_port_interest(
		uint32_t port,
		boost::shared_ptr<VirtualSwitch> switch_pointer) {
	needed_ports[port].erase(switch_pointer);
}

void PhysicalSwitch::start() {
	// Start up the generic connection handling
	OpenflowConnection::start();

	// Send an featuresrequest
	fluid_msg::of13::FeaturesRequest features_message;
	send_message( features_message );

	// Request ports via multipart
	{
		fluid_msg::of13::MultipartRequestPortDescription port_description_message(
			0, // The xid will be set send_message
			0); // The only flag is the more the flag indicating more messages follow
		send_message( port_description_message );
	}

	// Request information about the meters via multipart
	{
		fluid_msg::of13::MultipartRequestMeterFeatures information_message(
			0, // The xid will be set send_message
			0); // The only flag is the more the flag indicating more messages follow
		send_message( information_message );
	}

	// Request information about the groups via multipart
	{
		fluid_msg::of13::MultipartRequestGroupFeatures information_message(
			0, // The xid will be set send_message
			0); // The only flag is the more the flag indicating more messages follow
		send_message( information_message );
	}

	// Create the rest of the initial rules
	create_static_rules();
	// Create the dynamic rules
	update_dynamic_rules();

	// Start sending topology discovery messages
	schedule_topology_discovery_message();

	BOOST_LOG_TRIVIAL(info) << *this << " started";
}

void PhysicalSwitch::stop() {
	// Stop the generic connection handling
	OpenflowConnection::stop();

	// Stop the topology discovery
	topology_discovery_timer.cancel();

	if( state == unregistered ) {
		hypervisor->unregister_physical_switch(id);
	}
	else {
		hypervisor->unregister_physical_switch(features.datapath_id,id);
	}

	// Stop all the discovered links
	for( auto& port : ports ) {
		auto link = port.second.link;
		if( link != nullptr ) link->stop();
	}

	// TODO check_online for all virtual_switches that depend on this one

	BOOST_LOG_TRIVIAL(info) << *this << " stopped";
}

void PhysicalSwitch::create_static_rules() {
	// Create the topology discovery forward rule
	make_topology_discovery_rule();

	// Create the error detection rules
	{
		// Create the flowmod
		fluid_msg::of13::FlowMod flowmod;
		flowmod.command(fluid_msg::of13::OFPFC_ADD);
		flowmod.priority(0);
		flowmod.cookie(2);
		flowmod.table_id(0);
		flowmod.buffer_id(OFP_NO_BUFFER);

		// Create the actions
		fluid_msg::of13::WriteActions write_actions;
		write_actions.add_action(
			new fluid_msg::of13::OutputAction(
				fluid_msg::of13::OFPP_CONTROLLER,
				fluid_msg::of13::OFPCML_NO_BUFFER));
		flowmod.add_instruction(write_actions);

		// Send the message
		send_message(flowmod);

		// Change the table number and do it again
		flowmod.table_id(1);
		flowmod.cookie(3);
		send_message(flowmod);
	}

	// Create the meters per slice
	// TODO Doesn't work with slices created after this physical switch
	for( const Slice& slice : hypervisor->get_slices() ) {
		fluid_msg::of13::MeterMod meter_mod;
		meter_mod.command(fluid_msg::of13::OFPMC_ADD);
		meter_mod.flags(fluid_msg::of13::OFPMF_PKTPS);
		meter_mod.meter_id(slice.get_id()+1); // TODO Document this better, meter id's start at 1
		meter_mod.add_band(
			new fluid_msg::of13::MeterBand(
				fluid_msg::of13::OFPMBT_DROP,
				slice.get_max_rate(),
				0)); // Burst needs to be 0 unless flag burst is used

		// Send the message
		send_message(meter_mod);
	}

	// Send a barrierrequest
}

void PhysicalSwitch::update_dynamic_rules() {
	// Forward traffic between switches, if a packet comes
	// in over a link with a connection to another switch
	for( auto& switch_it : hypervisor->get_physical_switches() ) {
		// Forwarding to this switch makes no sense
		if( switch_it.first == id ) continue;

		for( auto& port_it : ports ) {
			// If this port doesn't have a link continue
			if( port_it.second.link == nullptr ) continue;

			// Create the flowmod
			fluid_msg::of13::FlowMod flowmod;
			flowmod.command(fluid_msg::of13::OFPFC_ADD);
			flowmod.table_id(0);

			// Create the actions
			fluid_msg::of13::WriteActions write_actions;
			write_actions.add_action(
				new fluid_msg::of13::OutputAction(
					port_it.first,
					fluid_msg::of13::OFPCML_NO_BUFFER));
			flowmod.add_instruction(write_actions);

			// Send the message
			send_message(flowmod);
		}
	}

	// Forward new packets to the personal flowtables
	for( auto& port : needed_ports ) {
		// Check if the port exists, has a link and
		// is needed by exactly 1 virtual switch
		auto it = ports.find(port.first);
		if(
			it!=ports.end() &&
			it->second.link==nullptr &&
			port.second.size()==1 ) {
			// Create a flow rule that meters and forwards the packet
		}
	}
}

void PhysicalSwitch::handle_port( fluid_msg::of13::Port& port, uint8_t reason ) {
	// Create a port status update message
	fluid_msg::of13::PortStatus port_status_message;

	// Check if this is a new unknown port
	auto search = ports.find(port.port_no());
	if( search == ports.end() ) {
		if( reason == fluid_msg::of13::OFPPR_DELETE ) {
			// If this is a delete message for a port we didn't know about
			// don't do anything.
			return;
		}
		// This is a new port we didn't know about
		port_status_message.reason(fluid_msg::of13::OFPPR_ADD);
		// Create the port structure
		ports[port.port_no()].port_data = port_status_message.desc();
	}
	else {
		if( reason == fluid_msg::of13::OFPPR_DELETE ) {
			// Delete this port from the switch
			ports.erase(port.port_no());
			port_status_message.reason(fluid_msg::of13::OFPPR_DELETE);
		}
		else {
			port_status_message.reason(fluid_msg::of13::OFPPR_MODIFY);
		}
	}


	// Loop over the depended switches and make them check again
	for( auto switch_pointer : needed_ports[port.port_no()] ) {
		// TODO Rewrite the port number
		// Set the port data with the rewritten port number into the port status message
		port_status_message.desc( port );

		// Send the message to the virtual switch
		switch_pointer->send_message(port_status_message);
	}
}

void PhysicalSwitch::reset_distances() {
	dist.clear();
	next.clear();

	// Loop over the links and fill the dist and next maps
	for( auto& port : ports ) {
		if( port.second.link != nullptr ) {
			int other_switch = port.second.link->get_other_switch_id(id);
			set_distance(other_switch,1);
			set_next(other_switch,port.first);
		}
	}
}
int PhysicalSwitch::get_distance(int switch_id) {
	// If we have no stored distance to switch_id the
	// distance is infinite
	if( dist.find(switch_id) == dist.end() ) {
		return topology::infinite;
	}
	else {
		return dist[switch_id];
	}
}
void PhysicalSwitch::set_distance(int switch_id, int distance) {
	dist[switch_id] = distance;
}
uint32_t PhysicalSwitch::get_next(int switch_id) {
	if( next.find(switch_id) == next.end() ) {
		BOOST_LOG_TRIVIAL(error) << "Asked next switch while no route is found";
		return UINT32_MAX;
	}
	else {
		return next[switch_id];
	}
}
void PhysicalSwitch::set_next(int switch_id, uint32_t port_number) {
	next[switch_id] = port_number;
}

PhysicalSwitch::pointer PhysicalSwitch::shared_from_this() {
	return boost::static_pointer_cast<PhysicalSwitch>(
			OpenflowConnection::shared_from_this());
}

void PhysicalSwitch::handle_error(fluid_msg::of13::Error& error_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received error Type=" << error_message.err_type() << " Code=" << error_message.code();
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
	// Extract the data of this message
	fluid_msg::of13::InPort* in_port_tlv =
		(fluid_msg::of13::InPort*) packet_in_message
			.get_oxm_field(fluid_msg::of13::OFPXMT_OFB_IN_PORT);
	uint32_t in_port = in_port_tlv->value();

	if( packet_in_message.table_id()==0 ) {
		// This packet was generated from the hypervisor reserved table

		if( packet_in_message.cookie() == 1 ) {
			// This packet in was generated by the topology discovery rule
			handle_topology_discovery_packet_in(packet_in_message);
		}
		else {
			BOOST_LOG_TRIVIAL(error) << *this
				<< " received packet in via error detection rule on port " << in_port;
		}
	}
	else {
		BOOST_LOG_TRIVIAL(info) << *this << " received packet_in on port " << in_port;

		// Look at the metadata pipeline field to figure
		// out from what slice this packetin was generated.
		// TODO
	}
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

	// The handle port function does all the important stuff
	fluid_msg::of13::Port port = port_status_message.desc();
	handle_port( port, port_status_message.reason() );
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

void PhysicalSwitch::handle_multipart_request_desc(fluid_msg::of13::MultipartRequestDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_flow(fluid_msg::of13::MultipartRequestFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_aggregate(fluid_msg::of13::MultipartRequestAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_table(fluid_msg::of13::MultipartRequestTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_port_stats(fluid_msg::of13::MultipartRequestPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_queue(fluid_msg::of13::MultipartRequestQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_group(fluid_msg::of13::MultipartRequestGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_group_desc(fluid_msg::of13::MultipartRequestGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_group_features(fluid_msg::of13::MultipartRequestGroupFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_meter(fluid_msg::of13::MultipartRequestMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_meter_config(fluid_msg::of13::MultipartRequestMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_meter_features(fluid_msg::of13::MultipartRequestMeterFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_table_features(fluid_msg::of13::MultipartRequestTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_port_desc(fluid_msg::of13::MultipartRequestPortDescription& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_request_experimenter(fluid_msg::of13::MultipartRequestExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart request it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_desc(fluid_msg::of13::MultipartReplyDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_flow(fluid_msg::of13::MultipartReplyFlow& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_aggregate(fluid_msg::of13::MultipartReplyAggregate& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_table(fluid_msg::of13::MultipartReplyTable& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_port_stats(fluid_msg::of13::MultipartReplyPortStats& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_queue(fluid_msg::of13::MultipartReplyQueue& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_group(fluid_msg::of13::MultipartReplyGroup& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_group_desc(fluid_msg::of13::MultipartReplyGroupDesc& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_group_features(fluid_msg::of13::MultipartReplyGroupFeatures& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received group features";
	if( (multipart_reply_message.features().types()&(1<<fluid_msg::of13::OFPGT_ALL))==0 ) {
		BOOST_LOG_TRIVIAL(error) << *this << " switch doesn't support ALL group type needed for hypervisor " << multipart_reply_message.features().types();
	}
	if( (multipart_reply_message.features().types()&(1<<fluid_msg::of13::OFPGT_INDIRECT))==0 ) {
		BOOST_LOG_TRIVIAL(error) << *this << " switch doesn't support INDIRECT group type needed for hypervisor";
	}
	// TODO Check if the switch supports all actions
	// that the hypervisor needs per group
}
void PhysicalSwitch::handle_multipart_reply_meter(fluid_msg::of13::MultipartReplyMeter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_meter_config(fluid_msg::of13::MultipartReplyMeterConfig& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_meter_features(fluid_msg::of13::MultipartReplyMeterFeatures& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received meter features";
	if( (multipart_reply_message.meter_features().band_types()&fluid_msg::of13::OFPMBT_DROP) == 0 ) {
		BOOST_LOG_TRIVIAL(error) << *this << " switch doesn't support drop meter band type";
	}
	if( multipart_reply_message.meter_features().max_meter() < hypervisor->get_slices().size() ) {
		BOOST_LOG_TRIVIAL(error) << *this << " switch doesn't support enough meters";
	}
}
void PhysicalSwitch::handle_multipart_reply_table_features(fluid_msg::of13::MultipartReplyTableFeatures& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}
void PhysicalSwitch::handle_multipart_reply_port_desc(fluid_msg::of13::MultipartReplyPortDescription& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received multipart reply port description";

	// Just act as if we received all the ports via PortStatus add messages
	for( fluid_msg::of13::Port& port : multipart_reply_message.ports() ) {
		handle_port( port, fluid_msg::of13::OFPPR_ADD );
	}
}
void PhysicalSwitch::handle_multipart_reply_experimenter(fluid_msg::of13::MultipartReplyExperimenter& multipart_request_message) {
	BOOST_LOG_TRIVIAL(error) << *this << " received multipart reply it shouldn't";
}

void PhysicalSwitch::print_to_stream(std::ostream& os) const {
	os << "[PhysicalSwitch id=" << id << ", dpid=" << features.datapath_id << "]";
}
