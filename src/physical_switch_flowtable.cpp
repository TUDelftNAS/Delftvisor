#include "physical_switch.hpp"
#include "virtual_switch.hpp"
#include "slice.hpp"
#include "hypervisor.hpp"
#include "tag.hpp"

#include <boost/log/trivial.hpp>

#include <fstream>

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

	// Create the rule forwarding packets that come from the
	// controller as if they arrived over a shared link
	{
		// Create the flowmod
		fluid_msg::of13::FlowMod flowmod;
		flowmod.command(fluid_msg::of13::OFPFC_ADD);
		flowmod.priority(10);
		flowmod.cookie(fluid_msg::of13::OFPP_CONTROLLER);
		flowmod.table_id(0);
		flowmod.buffer_id(OFP_NO_BUFFER);

		// Add the in-port match to flowmod
		flowmod.add_oxm_field(
			new fluid_msg::of13::InPort(fluid_msg::of13::OFPP_CONTROLLER));

		// Create the goto table instruction
		flowmod.add_instruction(
			new fluid_msg::of13::GoToTable(1));

		// Send the message
		send_message(flowmod);
	}

	// Create the meters per slice
	// TODO Doesn't work with slices created after this physical switch
	if( hypervisor->get_use_meters() ) {
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
	}

	// Create the group that sends the packet back to the controller
	{
		fluid_msg::of13::GroupMod group_mod;
		group_mod.command(fluid_msg::of13::OFPGC_ADD);
		group_mod.group_type(fluid_msg::of13::OFPGT_INDIRECT);
		group_mod.group_id(0);

		// Create the bucket that forward
		fluid_msg::of13::Bucket bucket;
		bucket.weight(0);
		bucket.watch_port(fluid_msg::of13::OFPP_ANY);
		bucket.watch_group(fluid_msg::of13::OFPG_ANY);
		bucket.add_action(
			new fluid_msg::of13::OutputAction(
				fluid_msg::of13::OFPP_CONTROLLER,
				fluid_msg::of13::OFPCML_NO_BUFFER));
		group_mod.add_bucket(bucket);

		// Send the message
		send_message(group_mod);
	}

	// TODO Send a barrierrequest
}

void PhysicalSwitch::update_dynamic_rules() {
	BOOST_LOG_TRIVIAL(info) << *this << " updating dynamic flow rules";

	// Update the port rules, there are 2 set of rules that are maintained
	// here. The rules in table 0 with priority 10 determining what to do
	// with packets that arrive over a certain link and the rules in table 1
	// with priority 10 determining what to do with packets that have
	// arrived over a link and want to be send out over a port on this switch.
	for( auto& port_pair : ports ) {
		// Alias the values that are iterated over
		const uint32_t& port_no = port_pair.first;
		Port& port = port_pair.second;

		// Start building the message to update table 0
		fluid_msg::of13::FlowMod flowmod_0;
		flowmod_0.priority(10);
		flowmod_0.cookie(port_no);
		flowmod_0.table_id(0);
		flowmod_0.buffer_id(OFP_NO_BUFFER);

		// If there is no rule about this port add a rule
		// in table 1
		fluid_msg::of13::FlowMod flowmod_1;
		flowmod_1.priority(10);
		flowmod_1.cookie(port_no);
		flowmod_1.table_id(1);
		flowmod_1.buffer_id(OFP_NO_BUFFER);

		// Determine what the current state of the forwarding rule
		// should be.
		Port::State current_state;
		// The virtual switch id in case this port has state host
		unsigned int virtual_switch_id;
		unsigned int slice_id;
		if( port.link != nullptr ) {
			// If this port has a link is that it's state
			current_state = Port::State::link_rule;
		}
		else {
			auto needed_it = needed_ports.find(port_no);
			if(
				needed_it!=needed_ports.end() &&
				needed_it->second.size()==1
			) {
				// A port can only be a host port if exactly 1 virtual
				// switch is interested in that port and it has no link
				current_state = Port::State::host_rule;
				// Extract the id of the virtual switch, there is
				// likely a better way to extract something from a set if
				// you know there is only 1 item, but this works
				auto& needed_port = (needed_it->second.begin())->second;
				virtual_switch_id = needed_port.virtual_switch->get_id();
				slice_id          = needed_port.virtual_switch->get_slice()->get_id();
			}
			else {
				// In all other occasions this port should go down
				current_state = Port::State::drop_rule;
			}
		}

		// Look what state this port had previously
		const Port::State prev_state = port.state;

		BOOST_LOG_TRIVIAL(trace) << *this
			<< " Looping over port " << port_no
			<< " prev=" << Port::state_to_string(prev_state)
			<< " curr=" << Port::state_to_string(current_state)
			<< " for port " << port_no;

		if( prev_state == Port::State::no_rule ) {
			// There is no rule known about this port
			flowmod_0.command(fluid_msg::of13::OFPFC_ADD);
			flowmod_1.command(fluid_msg::of13::OFPFC_ADD);
		}
		else {
			// If the state hasn't changed don't send any flowmod
			if( prev_state == current_state ) {
				continue;
			}
			flowmod_0.command(fluid_msg::of13::OFPFC_MODIFY_STRICT);
			flowmod_1.command(fluid_msg::of13::OFPFC_MODIFY_STRICT);
		}

		// Save the updated state
		port.state = current_state;

		BOOST_LOG_TRIVIAL(trace) << *this << " Updating port rule for port "
			<< port_no << " to " << Port::state_to_string(current_state);

		// Add the in-port match to flowmod_0
		flowmod_0.add_oxm_field(
			new fluid_msg::of13::InPort(port_no));

		// Add the necessary actions to flowmod_0
		if( current_state == Port::State::link_rule ) {
			flowmod_0.add_instruction(
				new fluid_msg::of13::GoToTable(1));
		}
		else if( current_state == Port::State::host_rule ) {
			// Add the meter instruction
			if( hypervisor->get_use_meters() ) {
				flowmod_0.add_instruction(
					new fluid_msg::of13::Meter(
						slice_id+1));
			}
			// Goto the tenant tables
			flowmod_0.add_instruction(
				new fluid_msg::of13::GoToTable(2));
			// Add the metadata write instruction
			MetadataTag metadata_tag;
			metadata_tag.set_group(false);
			metadata_tag.set_virtual_switch(virtual_switch_id);
			metadata_tag.add_to_instructions(flowmod_0);
		}
		else {
			// If current_state==Port::State::drop_rule don't add any actions
		}

		// Send the first message
		send_message(flowmod_0);

		// Flowmod 1 needs to be duplicated for each slice in the Hypervisor
		for( const Slice& slice : hypervisor->get_slices() ) {
			// Copy the already constructed message for each slice
			fluid_msg::of13::FlowMod flowmod_1_copy(flowmod_1);

			// Add the match to flowmod_1_copy
			VLANTag vlan_tag;
			vlan_tag.set_switch(id);
			vlan_tag.set_port(port_no);
			vlan_tag.set_slice(slice.get_id());
			vlan_tag.add_to_match(flowmod_1_copy);

			// Set the actions for flowmod_1_copy
			fluid_msg::of13::WriteActions write_actions;
			if( current_state == Port::State::host_rule ) {
				// Remove the VLAN Tag before forwarding to a host
				write_actions.add_action(
					new fluid_msg::of13::PopVLANAction());
			}
			else if( current_state == Port::State::link_rule ) {
				// Rewrite the port VLAN Tag to a shared link tag
				VLANTag vlan_tag;
				vlan_tag.set_switch(VLANTag::max_switch_id);
				vlan_tag.set_port(VLANTag::max_port_id);
				vlan_tag.set_slice(slice.get_id());
				vlan_tag.add_to_actions(write_actions);
			}
			// TODO What about drop rule?
			write_actions.add_action(
				new fluid_msg::of13::OutputAction(
					port_no,
					fluid_msg::of13::OFPCML_NO_BUFFER));
			flowmod_1_copy.add_instruction(write_actions);

			// Send the flowmods
			send_message(flowmod_1_copy);
		}
	}

	// Update shared link forwarding rules, the rules in table 1 with id 30
	for( auto& needed_port_pair : needed_ports ) {
		const uint32_t& port_no = needed_port_pair.first;

		// Not every needed port actually exists on this switch
		auto port_it = ports.find(needed_port_pair.first);
		if( port_it == ports.end() ) {
			continue;
		}
		const Port& port = port_it->second;

		// Loop over all virtual switches that need this port
		for( auto& needed_port_pair_2 : needed_port_pair.second ) {
			NeededPort& needed_port = needed_port_pair_2.second;

			// Start building the flowmod
			fluid_msg::of13::FlowMod flowmod;
			flowmod.table_id(1);
			flowmod.priority(30);
			flowmod.buffer_id(OFP_NO_BUFFER);

			// Figure out if we need to create/delete the rule
			if( !needed_port.rule_installed && port.state==Port::State::link_rule) {
				flowmod.command(fluid_msg::of13::OFPFC_ADD);
				needed_port.rule_installed = true;
			}
			else if( needed_port.rule_installed && port.state!=Port::State::link_rule ) {
				flowmod.command(fluid_msg::of13::OFPFC_DELETE_STRICT);
				needed_port.rule_installed = false;
			}
			else {
				continue;
			}

			BOOST_LOG_TRIVIAL(trace) << *this
				<< " Update rule 1,30 with port state="
				<< Port::state_to_string(port.state)
				<< " for port " << port_no;

			// Create the match
			VLANTag vlan_tag;
			vlan_tag.set_switch(VLANTag::max_switch_id);
			vlan_tag.set_port(VLANTag::max_port_id);
			vlan_tag.set_slice(needed_port.virtual_switch->get_slice()->get_id());
			vlan_tag.add_to_match(flowmod);
			flowmod.add_oxm_field(
				new fluid_msg::of13::InPort(port_no));

			// Add the actions
			fluid_msg::of13::ApplyActions apply_actions;
			apply_actions.add_action(
				new fluid_msg::of13::PopVLANAction());
			flowmod.add_instruction(apply_actions);
			// Add the meter instruction
			if( hypervisor->get_use_meters() ) {
				flowmod.add_instruction(
					new fluid_msg::of13::Meter(
						needed_port.virtual_switch->get_slice()->get_id()+1));
			}
			MetadataTag metadata_tag;
			metadata_tag.set_group(false);
			metadata_tag.set_virtual_switch(needed_port.virtual_switch->get_id());
			metadata_tag.add_to_instructions(flowmod);
			flowmod.add_instruction(
				new fluid_msg::of13::GoToTable(2));

			// Send the message
			send_message(flowmod);
		}
	}

	// Figure out what to do with traffic meant for a different switch
	for( const auto& switch_it : hypervisor->get_physical_switches() ) {
		int other_id = switch_it.first;

		// Forwarding to this switch makes no sense
		if( other_id == id ) continue;

		// If there is no path to this switch
		const auto next_it    = next.find(other_id);
		const auto current_it = current_next.find(other_id);
		bool next_exists      = next_it!=next.end();
		bool current_exists   = current_it!=current_next.end();

		// If this switch was and is unreachable skip this switch
		if( !next_exists && !current_exists ) continue;
		// If this switch is reachable but that rule is already set
		// in the switch
		if(
			next_exists && current_exists &&
			next_it->second==current_it->second
		) continue;

		// If we arrived here we need to update something in the switch.
		// Create the flowmod
		fluid_msg::of13::FlowMod flowmod;
		flowmod.table_id(1);
		flowmod.priority(20);
		flowmod.buffer_id(OFP_NO_BUFFER);

		if( !current_exists ) {
			flowmod.command(fluid_msg::of13::OFPFC_ADD);
		}
		else if( current_exists && next_exists ) {
			flowmod.command(fluid_msg::of13::OFPFC_MODIFY_STRICT);
		}
		else {
			flowmod.command(fluid_msg::of13::OFPFC_DELETE_STRICT);
		}

		if( next_exists ) {
			// Add the vlantag match field
			VLANTag vlan_tag;
			vlan_tag.set_switch(other_id);
			vlan_tag.add_to_match(flowmod);

			// Tell the packet to output over the correct port
			fluid_msg::of13::WriteActions write_actions;
			write_actions.add_action(
				new fluid_msg::of13::OutputAction(
					next_it->second,
					fluid_msg::of13::OFPCML_NO_BUFFER));
			flowmod.add_instruction(write_actions);
		}

		// Send the message
		send_message(flowmod);
	}

	// Loop over all virtual switches for which we have rewrite data
	for( auto& rewrite_entry_pair : rewrite_map ) {
		const int& virtual_switch_id        = rewrite_entry_pair.first;
		auto& rewrite_entry                 = rewrite_entry_pair.second;

		// Get the virtual switch for which we are going to update the
		const VirtualSwitch* virtual_switch =
			hypervisor->get_virtual_switch(virtual_switch_id);

		// If this switch is down skip these entries. This can happen when a
		// link goes down causing multiple virtual switches to fail. In that
		// case no next port is found towards the needed ports of that switch.
		if( virtual_switch->is_down() ) continue;

		// Loop over all ports on the virtual switch
		for( auto& port_pair : virtual_switch->get_port_to_physical_switch() ) {
			const uint32_t& virtual_port  = port_pair.first;
			const uint64_t& physical_dpid = port_pair.second;

			// Retrieve more references and pointers so we can easily use
			// those below
			PhysicalSwitch::pointer physical_switch =
				hypervisor->get_physical_switch_by_datapath_id(physical_dpid);
			OutputGroup& output_group =
				rewrite_entry.output_groups.at(virtual_port);

			// Determine what state this rule should have
			OutputGroup::State new_state;
			uint32_t new_output_port;

			// If it is a port on this switch
			if( physical_dpid == features.datapath_id ) {
				// Retrieve the mapping from local to physical port id
				const auto& port_map = virtual_switch->get_port_map(features.datapath_id);

				// Get the physical port id
				new_output_port = port_map.get_physical(virtual_port);

				auto port_it = ports.find(new_output_port);
				// If the port is not yet found or to a host
				if( port_it==ports.end() || port_it->second.link==nullptr ) {
					new_state = OutputGroup::State::host_rule;
				}
				// If the port is to a shared link
				else {
					new_state = OutputGroup::State::shared_link_rule;
				}
			}
			// If it is a port on another switch
			else {
				new_state       = OutputGroup::State::switch_rule;
				new_output_port = next.at(physical_switch->get_id());
			}

			// If the states and output ports are the same the group doesn't
			// need to be updated
			if( output_group.state==new_state &&
				output_group.output_port==new_output_port ) {
				continue;
			}

			// Create the new group to
			fluid_msg::of13::GroupMod group_mod;
			// If this group doesn't exist it is an add command
			if( output_group.state == OutputGroup::State::no_rule ) {
				group_mod.command(fluid_msg::of13::OFPGC_ADD);
			}
			// Otherwise it is an edit command
			else {
				group_mod.command(fluid_msg::of13::OFPGC_MODIFY);
			}
			group_mod.group_type(fluid_msg::of13::OFPGT_INDIRECT);
			group_mod.group_id(output_group.group_id);

			// Update the state and output port in the output_group
			output_group.state       = new_state;
			output_group.output_port = new_output_port;

			// Create the bucket to add to the group mod
			fluid_msg::of13::Bucket bucket;
			bucket.weight(0);
			bucket.watch_port(fluid_msg::of13::OFPP_ANY);
			bucket.watch_group(fluid_msg::of13::OFPG_ANY);

			// Determine what actions to add to the bucket and do it
			fluid_msg::ActionSet action_set;
			if( new_state == OutputGroup::State::host_rule ) {
				action_set.add_action(
					new fluid_msg::of13::OutputAction(
						new_output_port,
						fluid_msg::of13::OFPCML_NO_BUFFER));
			}
			else if( new_state == OutputGroup::State::shared_link_rule ) {
				// Push the VLAN Tag
				action_set.add_action(
					new fluid_msg::of13::PushVLANAction(0x8100));

				// Set the data in the VLAN Tag
				VLANTag vlan_tag;
				vlan_tag.set_switch(VLANTag::max_switch_id);
				vlan_tag.set_port(VLANTag::max_port_id);
				vlan_tag.set_slice(virtual_switch->get_slice()->get_id());
				vlan_tag.add_to_actions(action_set);

				// Output the packet over the proper port
				action_set.add_action(
					new fluid_msg::of13::OutputAction(
						new_output_port,
						fluid_msg::of13::OFPCML_NO_BUFFER));
			}
			// Otherwise the state needs to be switch_rule
			else {
				// Push the VLAN Tag
				action_set.add_action(
					new fluid_msg::of13::PushVLANAction(0x8100));

				// Get the port id on the foreign switch
				uint32_t foreign_output_port =
					virtual_switch
						->get_port_map(physical_dpid)
							.get_physical(virtual_port);

				// Set the data in the VLAN Tag
				VLANTag vlan_tag;
				vlan_tag.set_switch(physical_switch->get_id());
				vlan_tag.set_port(foreign_output_port);
				vlan_tag.set_slice(virtual_switch->get_slice()->get_id());
				vlan_tag.add_to_actions(action_set);

				// Output the packet over the proper port
				action_set.add_action(
					new fluid_msg::of13::OutputAction(
						new_output_port,
						fluid_msg::of13::OFPCML_NO_BUFFER));
			}

			// Add the bucket
			bucket.actions(action_set);
			group_mod.add_bucket(bucket);

			// Send the message
			send_message(group_mod);
		}
	}

	// TODO Remove this section
	if( state == registered ) {
		std::ostringstream string_stream;
		string_stream << "dpid_" << features.datapath_id << ".dat";
		std::ofstream file(string_stream.str());
		print_detailed(file);
		BOOST_LOG_TRIVIAL(trace) << "Wrote to file " << string_stream.str();
	}
}

void PhysicalSwitch::print_detailed(std::ostream& os) const {
	print_to_stream(os); os << " = {\n";
	os << "\tports = [\n";
	for( auto port_pair : ports ) {
		os << "\t\t{\n";
		os << "\t\t\tid = " << port_pair.first << "\n";
		os << "\t\t\tstate = " << Port::state_to_string(port_pair.second.state) << "\n";
		os << "\t\t\tneeded-ports = { ";
		auto needed_port_it = needed_ports.find(port_pair.first);
		if( needed_port_it != needed_ports.end() ) {
			for( auto vs_ptr_pair : needed_port_it->second ) {
				os << *(vs_ptr_pair.second.virtual_switch) << " ";
			}
		}
		os << "}\n";
		os << "\t\t}\n";
	}
	os << "\t]\n";
	os << "\trewrite-map = [\n";
	for( auto rewrite_map_pair : rewrite_map ) {
		os << "\t\t{\n";
		os << "\t\t\tvirtual-switch-id = " << rewrite_map_pair.first << "\n";
		os << "\t\t\tflood-group-id = " << rewrite_map_pair.second.flood_group_id << "\n";
		os << "\t\t\tgroup-id-map = [\n";
		for( auto group_id_pair : rewrite_map_pair.second.group_id_map.get_virtual_to_physical() ) {
			os << "\t\t\t\t" << group_id_pair.first << " <=> " << group_id_pair.second << "\n";
		}
		os << "\t\t\t]\n";
		os << "\t\t\toutput-groups = [\n";
		for( auto output_group_pair : rewrite_map_pair.second.output_groups ) {
			os << "\t\t\t\t{\n";
			os << "\t\t\t\t\tvirtual-port-id = " << output_group_pair.first << "\n";
			os << "\t\t\t\t\tgroup-id = " << output_group_pair.second.group_id << "\n";
			os << "\t\t\t\t\toutput-port = " << output_group_pair.second.output_port << "\n";
			os << "\t\t\t\t\tstate = " << OutputGroup::state_to_string(output_group_pair.second.state) << "\n";
			os << "\t\t\t\t}\n";
		}
		os << "\t\t\t]\n";
		os << "\t\t}\n";
	}
	os << "\t]\n";
	os << "}\n";
}
