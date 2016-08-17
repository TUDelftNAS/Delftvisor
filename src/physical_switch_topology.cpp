#include "physical_switch.hpp"
#include "discoveredlink.hpp"
#include "hypervisor.hpp"
#include "rewrite.hpp"

#include <vector>

#include <boost/make_shared.hpp>
#include <boost/log/trivial.hpp>

void PhysicalSwitch::make_topology_discovery_rule() {
	// Create the flowmod
	fluid_msg::of13::FlowMod flowmod;
	flowmod.command(fluid_msg::of13::OFPFC_ADD);
	flowmod.table_id(0);
	flowmod.cookie(1);
	flowmod.priority(70);
	flowmod.buffer_id(OFP_NO_BUFFER);

	// Create the match
	flowmod.add_oxm_field(
		new fluid_msg::of13::VLANVid(
			rewrite::slice_bits(rewrite::max_slice_id) | fluid_msg::of13::OFPVID_PRESENT,
			rewrite::slice_mask | fluid_msg::of13::OFPVID_PRESENT));

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

void PhysicalSwitch::schedule_topology_discovery_message() {
	// If there are no ports registered yet wait 1 period
	int wait_time;
	if( ports.size() == 0 ) {
		wait_time = topology::period;
	}
	else {
		wait_time = topology::period/ports.size();
	}

	// Schedule the topology message to be send
	topology_discovery_timer.expires_from_now(
		boost::posix_time::milliseconds(wait_time));
	topology_discovery_timer.async_wait(
		boost::bind(
			&PhysicalSwitch::send_topology_discovery_message,
			shared_from_this(),
			boost::asio::placeholders::error));
}

// A random ARP packet with a VLAN tag. The VLAN id=0
std::vector<uint8_t> topology_discovery_packet = {
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x05, 0x02, 0x71, 0xfc, 0xdb, 0x81, 0x00, 0x00, 0x10,
0x00, 0x24, 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04,
0x00, 0x01, 0x00, 0x05, 0x02, 0x71, 0xfc, 0xdb, 0x83, 0x97, 0x14, 0x48, 0xff, 0xff, 0xff, 0xff,
0xff, 0xff, 0x83, 0x97, 0x14, 0xfe, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55
};

void PhysicalSwitch::send_topology_discovery_message(
		const boost::system::error_code& error) {
	if( error.value() == boost::asio::error::operation_aborted ) {
		BOOST_LOG_TRIVIAL(trace) << *this
			<< " topology discovery timer cancelled";
		return;
	}
	else if( error ) {
		BOOST_LOG_TRIVIAL(error) << *this
			<< " topology discovery timer error: " << error.message();
		return;
	}

	// Figure out what port to send the packet over
	// WONTFIX A port can be skipped right now if a port before
	// it is deleted
	topology_discovery_port = (topology_discovery_port+1)%ports.size();
	auto it = ports.begin();
	for( int i=0; i<topology_discovery_port; ++i ) ++it;
	uint32_t port_number = it->first;

	// Create the data and mask with the slice/switch/port information
	uint32_t vlan_id =
		rewrite::slice_bits(rewrite::max_slice_id) |
		rewrite::switch_bits(id) |
		rewrite::port_bits(port_number);
	// Add the CFI bit
	vlan_id = (vlan_id&rewrite::make_mask(12)) |
		(1<<12) |
		((vlan_id&(rewrite::make_mask(3)<<12))<<1);
	// Set the vlan values in the packet
	topology_discovery_packet[14] = (vlan_id>>8) & 0xff;
	topology_discovery_packet[15] = vlan_id & 0xff;

	// Create the packet out message
	fluid_msg::of13::PacketOut packet_out;
	packet_out.buffer_id( OFP_NO_BUFFER );
	packet_out.data(
		&topology_discovery_packet[0],
		topology_discovery_packet.size());
	packet_out.add_action(
		new fluid_msg::of13::OutputAction(
			port_number,
			fluid_msg::of13::OFPCML_NO_BUFFER));

	BOOST_LOG_TRIVIAL(trace) << *this <<
		" sending topology discovery packet on port " << port_number;

	// Send the message
	send_message(packet_out);

	// Schedule the next message
	schedule_topology_discovery_message();
}

void PhysicalSwitch::handle_topology_discovery_packet_in(
	fluid_msg::of13::PacketIn& packet_in_message) {
	// Extract the port of this message
	fluid_msg::of13::InPort* in_port_tlv =
		(fluid_msg::of13::InPort*) packet_in_message
			.get_oxm_field(fluid_msg::of13::OFPXMT_OFB_IN_PORT);
	uint32_t in_port = in_port_tlv->value();

	// Try to parse the packet to see if it has a VLAN tag
	struct EthernetHeader {
		uint8_t mac_dst[6];
		uint8_t mac_src[6];
		uint8_t ether_type[2];
		uint8_t vlan_tag[2];
	};
	EthernetHeader * packet = (EthernetHeader*) packet_in_message.data();
	// Interpret the vlan tag disregarding endianness
	uint32_t vlan_id = packet->vlan_tag[0]*256+packet->vlan_tag[1];
	// Remove the 12th bit, this is the CFI bit and is always 1
	vlan_id = (vlan_id&rewrite::make_mask(12)) |
		((vlan_id>>1)&(rewrite::make_mask(3)<<12));

	// Extract the relevant information from the VLAN tag
	int switch_num = rewrite::extract_switch_id(vlan_id);
	int slice      = rewrite::extract_slice_id(vlan_id);
	uint32_t port  = rewrite::extract_port_id(vlan_id);

	// Extract the slice id to see if this is for topology discovery
	BOOST_LOG_TRIVIAL(trace) << *this
		<< " received topology discovery packet_in";
	BOOST_LOG_TRIVIAL(trace) << *this
		<< "\t sw=" << switch_num << " sl=" << slice << " p=" << port;

	// If this doesn't use the slice reserved for topology discovery
	if( slice != rewrite::max_slice_id ) {
		BOOST_LOG_TRIVIAL(error) << *this << " received topology discovery packet in with wrong slice: " << slice;
		return;
	}

	// Determine if this link already exists
	auto it = ports.find(in_port);
	if( it->second.link == nullptr ) {
		// Create a discovered link
		auto discovered_link = boost::make_shared<DiscoveredLink>(
			socket.get_io_service(),
			hypervisor,
			id,
			in_port,
			switch_num,
			port);

		// Add the link to the switches
		auto switch_2_pointer = hypervisor
			->get_physical_switch(switch_num);
		if( switch_2_pointer == nullptr ) {
			BOOST_LOG_TRIVIAL(error) << discovered_link << " cannot construct link to not existing switch";
			return;
		}
		this->add_link( discovered_link );
		switch_2_pointer->add_link(discovered_link);

		// Start the timer on the link
		discovered_link->reset_timer();

		// Recalculate the routes with this extra link
		hypervisor->calculate_routes();
	}
	else {
		// Reset the liveness timer on this link
		it->second.link->reset_timer();
	}
}

void PhysicalSwitch::add_link(boost::shared_ptr<DiscoveredLink> discovered_link) {
	uint32_t discovered_port = discovered_link->get_port_number(id);
	auto it = ports.find(discovered_port);
	if( it == ports.end() ) {
		BOOST_LOG_TRIVIAL(error) << *this << " tried to add link from a port that doesn't exist";
		discovered_link->stop();
	}
	else {
		it->second.link = discovered_link;
	}
}

void PhysicalSwitch::reset_link(boost::shared_ptr<DiscoveredLink> discovered_link) {
	uint32_t discovered_port = discovered_link->get_port_number(id);
	auto it = ports.find(discovered_port);
	if( it == ports.end() ) {
		BOOST_LOG_TRIVIAL(error) << *this << " tried to remove link from a port that doesn't exist";
	}
	else {
		it->second.link.reset();
	}
	// This function is called when a discovered link times out.
	// Since both ends of that link has to be removed it doesn't
	// make sense to do the recalculation of the routes here.
}
