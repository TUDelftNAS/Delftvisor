#include "physical_switch.hpp"
#include "virtual_switch.hpp"
#include "hypervisor.hpp"
#include "discoveredlink.hpp"

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

const std::unordered_map<uint32_t,PhysicalPort>& PhysicalSwitch::get_ports() const
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
	fluid_msg::of13::FeaturesRequest features_message(get_next_xid());
	send_message( features_message );

	// Request ports via multipart
	// TODO

	// Create the rest of the initial rules
	create_initial_rules();
	// Create the dynamic rules
	update_rules();

	// Start sending topology discovery messages
	schedule_topology_discovery_message();

	BOOST_LOG_TRIVIAL(info) << *this << " started";
}

void PhysicalSwitch::stop() {
	// Stop the generic connection handling
	OpenflowConnection::stop();

	topology_discovery_timer.cancel();

	if( state == unregistered ) {
		hypervisor->unregister_physical_switch(id);
	}
	else {
		hypervisor->unregister_physical_switch(features.datapath_id,id);
	}

	// TODO check_online for all virtual_switches that depend on this one

	BOOST_LOG_TRIVIAL(info) << *this << " stopped";
}

void PhysicalSwitch::create_initial_rules() {
	// Create the topology discovery forward rule
	{
		// Create the flowmod
		fluid_msg::of13::FlowMod flowmod;
		flowmod.xid(get_next_xid());
		flowmod.command(fluid_msg::of13::OFPFC_ADD);
		flowmod.table_id(0);
		flowmod.priority(70);

		// Create the match
		// TODO
		//flowmod.add_oxm_field();

		// Create the action
		fluid_msg::of13::WriteActions write_actions;
		write_actions.add_action(
			new fluid_msg::of13::OutputAction(
				fluid_msg::of13::OFPP_CONTROLLER,
				fluid_msg::of13::OFPCML_NO_BUFFER));
		flowmod.add_instruction(write_actions);

		// Send the message
		send_message(flowmod);
	}

	// Create the error detection rule
	{
		// Create the flowmod
		fluid_msg::of13::FlowMod flowmod;
		flowmod.xid(get_next_xid());
		flowmod.command(fluid_msg::of13::OFPFC_ADD);
		flowmod.cookie(3);
		flowmod.table_id(0);

		// Create the actions
		fluid_msg::of13::WriteActions write_actions;
		write_actions.add_action(
			new fluid_msg::of13::OutputAction(
				fluid_msg::of13::OFPP_CONTROLLER,
				fluid_msg::of13::OFPCML_NO_BUFFER));
		flowmod.add_instruction(write_actions);

		// Send the message
		send_message(flowmod);
	}

	// Create the meters per slice
	// TODO Doesn't work with slices created after this physical switch
	for( const Slice& slice : hypervisor->get_slices() ) {
		fluid_msg::of13::MeterMod meter_mod;
		meter_mod.xid(get_next_xid());
		meter_mod.command(fluid_msg::of13::OFPMC_ADD);
		meter_mod.flags(fluid_msg::of13::OFPMF_PKTPS);
		meter_mod.meter_id(slice.get_id());
		meter_mod.add_band(
			new fluid_msg::of13::MeterBand(
				fluid_msg::of13::OFPMBT_DROP,
				slice.get_max_rate(),
				1)); // TODO What does burst_size mean?

		// Send the message
		send_message(meter_mod);
	}

	// Send a barrierrequest
}

void PhysicalSwitch::update_rules() {
	// Forward traffic between switches, if a packet comes
	// in over a link with a connection to another switch
	for( auto switch_it : hypervisor->get_physical_switches() ) {
		// Forwarding to this switch makes no sense
		if( switch_it.first == id ) continue;

		for( auto port_it : ports ) {
			// If this port doesn't have a link continue
			if( port_it.second.link == nullptr ) continue;

			// Create the flowmod
			fluid_msg::of13::FlowMod flowmod;
			flowmod.xid(get_next_xid());
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
	for( auto port : needed_ports ) {
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

void PhysicalSwitch::schedule_topology_discovery_message() {
	// If there are no ports registered yet wait 1 period
	int wait_time;
	if( ports.size() == 0 ) {
		wait_time = topology::period;
	}
	else {
		wait_time = topology::period/ports.size();
	}

	topology_discovery_timer.expires_from_now(
		boost::posix_time::milliseconds(wait_time));
	topology_discovery_timer.async_wait(
		boost::bind(
			&PhysicalSwitch::send_topology_discovery_message,
			shared_from_this(),
			boost::asio::placeholders::error));
}

void PhysicalSwitch::send_topology_discovery_message(const boost::system::error_code& error) {
	if( error.value() == boost::asio::error::operation_aborted ) {
		BOOST_LOG_TRIVIAL(trace) << *this << " topology discovery timer cancelled";
		return;
	}
	else if( error ) {
		BOOST_LOG_TRIVIAL(error) << *this << " topology discovery timer error: " << error.message();
		return;
	}

	// Figure out what port to send the packet over
	// Check if the port still exists!
	// TODO

	// Create the packet out message
	fluid_msg::of13::PacketOut packet_out;
	packet_out.xid(get_next_xid());
	// TODO Create the actual message and put it into the PacketOut

	// Send the message
	//send_message(packet_out);

	// Schedule the next message
	schedule_topology_discovery_message();
}

void PhysicalSwitch::reset_link(uint32_t port_number) {
	ports[port_number].link.reset();
	// This function is called when a discovered link times out.
	// Since both ends of that link has to be removed it doesn't
	// make sense to do the recalculation of the routes here.
}

void PhysicalSwitch::reset_distances() {
	dist.clear();
	next.clear();

	// Loop over the links and fill the dist and next maps
	for( auto port : ports ) {
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
	BOOST_LOG_TRIVIAL(info) << *this << " received error Code=" << error_message.code() << " Type=" << error_message.err_type();
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
	// Check if this is a new unknown port
	if( search == ports.end() ) {
		if( port_status_message.reason() == fluid_msg::of13::OFPPR_DELETE ) {
			// If this is a delete message for a port we didn't know about
			// don't do anything.
			return;
		}
		// This is a new port we didn't know about
		port_status_message.reason(fluid_msg::of13::OFPPR_ADD);
		// Create the port structure
		uint32_t port_number = port_status_message.desc().port_no();
		ports[port_number].port_data = port_status_message.desc();
	}
	else {
		if( port_status_message.reason() == fluid_msg::of13::OFPPR_DELETE ) {
			// Delete this port from the switch
			ports.erase(port_number);
		}
	}

	// Loop over the depended switches and make them check again
	for( auto switch_pointer : needed_ports[port_number] ) {
		// TODO Rewrite the port number
		switch_pointer->send_message(port_status_message);
		switch_pointer->check_online();
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
