#include "physical_switch.hpp"
#include "virtual_switch.hpp"
#include "hypervisor.hpp"
#include "discoveredlink.hpp"
#include "slice.hpp"

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

const struct PhysicalSwitch::Features& PhysicalSwitch::get_features() const {
	return features;
}

const std::unordered_map<uint32_t,PhysicalSwitch::Port>& PhysicalSwitch::get_ports() const {
	return ports;
}

void PhysicalSwitch::register_port_interest(
		uint32_t port,
		boost::shared_ptr<VirtualSwitch> switch_pointer) {
	BOOST_LOG_TRIVIAL(trace) << *this << " interest was registered for port " << port;
	needed_ports[port].insert(switch_pointer);
}
void PhysicalSwitch::remove_port_interest(
		uint32_t port,
		boost::shared_ptr<VirtualSwitch> switch_pointer) {
	BOOST_LOG_TRIVIAL(trace) << *this << " interest was unregistered for port " << port;
	needed_ports.at(port).erase(switch_pointer);
}

void PhysicalSwitch::send_request_message(
		fluid_msg::OFMsg& message,
		boost::weak_ptr<VirtualSwitch> virtual_switch) {
	uint32_t xid = send_message(message);
	xid_map[xid].original_xid   = message.xid();
	xid_map[xid].virtual_switch = virtual_switch;
}

void PhysicalSwitch::start() {
	// Start up the generic connection handling
	OpenflowConnection::start();

	// Send an featuresrequest
	{
		fluid_msg::of13::FeaturesRequest features_message;
		send_message( features_message );
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

	// Request ports via multipart
	{
		fluid_msg::of13::MultipartRequestPortDescription port_description_message(
			0, // The xid will be set send_message
			0); // The only flag is the more the flag indicating more messages follow
		send_message( port_description_message );
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

	// Remove this switch from the registry
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

	// Let the entire network recalculate, this is done to assure
	// that a virtual switch that only depended on this switch also
	// gets stopped.
	hypervisor->calculate_routes();

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
		ports[port.port_no()].port_data = port;
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
	auto switch_pointers = needed_ports.find(port.port_no());
	if( switch_pointers != needed_ports.end() ) {
		BOOST_LOG_TRIVIAL(trace) << *this << "PortStatus port=" << switch_pointers->first << " dep_sw_amount=" << switch_pointers->second.size();

		for( auto& switch_pointer : switch_pointers->second ) {
			// Skip if this virtual switch is not online
			if( !switch_pointer->is_connected() ) continue;

			BOOST_LOG_TRIVIAL(trace) << *this << "\tPortStatus dpid=" << features.datapath_id << ", port_no=" << port.port_no();

			// Rewrite the port number
			port.port_no(
				switch_pointer->
					get_virtual_port_no(features.datapath_id,port.port_no()));
			// Set the port data with the rewritten port number into the port status message
			port_status_message.desc( port );

			BOOST_LOG_TRIVIAL(trace) << *this << "\tPortStatus rewritten port_no=" << port.port_no();

			// Send the message to the virtual switch
			switch_pointer->send_message(port_status_message);
		}
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
		return dist.at(switch_id);
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
		return next.at(switch_id);
	}
}
void PhysicalSwitch::set_next(int switch_id, uint32_t port_number) {
	next[switch_id] = port_number;
}

PhysicalSwitch::pointer PhysicalSwitch::shared_from_this() {
	return boost::static_pointer_cast<PhysicalSwitch>(
			OpenflowConnection::shared_from_this());
}

void PhysicalSwitch::print_to_stream(std::ostream& os) const {
	os << "[PhysicalSwitch id=" << id << ", dpid=" << features.datapath_id << "]";
}

void PhysicalSwitch::handle_error(fluid_msg::of13::Error& error_message) {
	BOOST_LOG_TRIVIAL(info) << *this
		<< " received error Type=" << error_message.err_type()
		<< " Code=" << error_message.code();
	// TODO
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

void PhysicalSwitch::handle_config_reply(fluid_msg::of13::GetConfigReply& config_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received get_config_reply";

	features.flags         = config_reply_message.flags();
	features.miss_send_len = config_reply_message.miss_send_len();
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

void PhysicalSwitch::handle_multipart_reply_meter_features(fluid_msg::of13::MultipartReplyMeterFeatures& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received meter features";
	if( (multipart_reply_message.meter_features().band_types()&fluid_msg::of13::OFPMBT_DROP) == 0 ) {
		BOOST_LOG_TRIVIAL(error) << *this << " switch doesn't support drop meter band type";
	}
	if( multipart_reply_message.meter_features().max_meter() < hypervisor->get_slices().size() ) {
		BOOST_LOG_TRIVIAL(error) << *this << " switch doesn't support enough meters";
	}
}

void PhysicalSwitch::handle_multipart_reply_port_desc(fluid_msg::of13::MultipartReplyPortDescription& multipart_reply_message) {
	BOOST_LOG_TRIVIAL(info) << *this << " received multipart reply port description";

	// Just act as if we received all the ports via PortStatus add messages
	for( fluid_msg::of13::Port& port : multipart_reply_message.ports() ) {
		handle_port( port, fluid_msg::of13::OFPPR_ADD );
	}
}
