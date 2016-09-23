#include "physical_switch.hpp"
#include "virtual_switch.hpp"

#include "tag.hpp"

#include <boost/log/trivial.hpp>

bool PhysicalSwitch::rewrite_instruction_set(
		fluid_msg::of13::InstructionSet& old_instruction_set,
		fluid_msg::of13::InstructionSet& instruction_set_with_output,
		fluid_msg::of13::InstructionSet& instruction_set_without_output,
		VirtualSwitch* virtual_switch) {
	uint64_t metadata_tag  = 0;
	uint64_t metadata_mask = 0;

	// Loop over all the instructions in the original set
	for( fluid_msg::of13::Instruction* instruction : old_instruction_set.instruction_set() ) {
		if( instruction->type() == fluid_msg::of13::OFPIT_GOTO_TABLE ) {
			fluid_msg::of13::GoToTable* goto_table =
				(fluid_msg::of13::GoToTable*) instruction;

			// TODO Check if goto_table->table_id()+2 is within physical
			// switch capabilities

			instruction_set_with_output.add_instruction(
				new fluid_msg::of13::GoToTable(goto_table->table_id()+2));
			instruction_set_without_output.add_instruction(
				new fluid_msg::of13::GoToTable(goto_table->table_id()+2));
		}
		else if( instruction->type() == fluid_msg::of13::OFPIT_WRITE_METADATA ) {
			fluid_msg::of13::WriteMetadata* write_metadata =
				(fluid_msg::of13::WriteMetadata*) instruction;

			constexpr int total_bits = MetadataTag::num_virtual_switch_bits + 1;

			// Check if bits in the mask are set that would be shifted out
			constexpr uint64_t mask_check
				= make_mask(total_bits) << (64-total_bits);
			if( write_metadata->metadata_mask() & mask_check ) {
				BOOST_LOG_TRIVIAL(warning) << *this
					<< " metadata instruction uses reserved bits";
				return false;
			}

			// Add the shifted metadata tag information to the new tag
			metadata_tag  |= write_metadata->metadata()      << total_bits;
			metadata_mask |= write_metadata->metadata_mask() << total_bits;
		}
		else if( instruction->type() == fluid_msg::of13::OFPIT_WRITE_ACTIONS ) {
			fluid_msg::of13::WriteActions* write_actions =
				(fluid_msg::of13::WriteActions*) instruction;

			fluid_msg::ActionSet old_action_set = write_actions->actions();
			fluid_msg::ActionSet action_set_with_output, action_set_without_output;

			// Rewrite the action sets
			bool has_action_with_group = false;
			if( !rewrite_action_set(
					old_action_set,
					action_set_with_output,
					action_set_without_output,
					has_action_with_group,
					virtual_switch) ) {
				return false;
			}

			// If a group action was used set the metadata group bit
			if( has_action_with_group ) {
				metadata_tag  |= 1;
				metadata_mask |= 1;
			}

			// Create new instructions in the appropiate instruction sets
			// TODO What happens if an action set has no actions in it?
			instruction_set_with_output.add_instruction(
				new fluid_msg::of13::WriteActions(action_set_with_output));
			instruction_set_without_output.add_instruction(
				new fluid_msg::of13::WriteActions(action_set_without_output));
		}
		else if( instruction->type() == fluid_msg::of13::OFPIT_APPLY_ACTIONS ) {
			fluid_msg::of13::ApplyActions* apply_actions =
				(fluid_msg::of13::ApplyActions*) instruction;

			fluid_msg::ActionList old_action_list = apply_actions->actions();
			fluid_msg::ActionList new_action_list;

			// Rewrite the actions list
			if( !rewrite_action_list(
					old_action_list,
					new_action_list,
					virtual_switch) ) {
				return false;
			}

			// Create a new instruction in both instruction sets
			instruction_set_with_output.add_instruction(
				new fluid_msg::of13::ApplyActions(new_action_list));
			instruction_set_without_output.add_instruction(
				new fluid_msg::of13::ApplyActions(new_action_list));
		}
		else if( instruction->type() == fluid_msg::of13::OFPIT_CLEAR_ACTIONS ) {
			// Copy the instruction in the both action sets
			instruction_set_with_output.add_instruction(instruction->clone());
			instruction_set_without_output.add_instruction(instruction->clone());

			// Set the first bit in the metadata mask so the group bit gets
			// overwritten with a 0. If there also is a write action instruction
			// in this instruction set has the metadata_tag and metadata_mask
			// value already been set, the clear-action instruction is executed
			// before the write-action instruction. In that case the below statement
			// doesn't actually change anything which is correct.
			metadata_mask |= 1;
		}
		else if( instruction->type() == fluid_msg::of13::OFPIT_METER ) {
			BOOST_LOG_TRIVIAL(warning) << *this
				<< " received flowmod with meter instruction";
			return false;
		}
		else if( instruction->type() == fluid_msg::of13::OFPIT_EXPERIMENTER ) {
			BOOST_LOG_TRIVIAL(warning) << *this
				<< " received flowmod with experimenter instruction";
			return false;
		}
		else {
			// TODO Remove this case
			instruction_set_with_output.add_instruction(instruction->clone());
			instruction_set_without_output.add_instruction(instruction->clone());
		}
	}

	// If any information was set in the metadata mask we need to add
	// the metadata instruction to both instruction sets
	if( metadata_mask != 0 ) {
		instruction_set_with_output.add_instruction(
			new fluid_msg::of13::WriteMetadata(
				metadata_tag,
				metadata_mask));
		instruction_set_without_output.add_instruction(
			new fluid_msg::of13::WriteMetadata(
				metadata_tag,
				metadata_mask));
	}

	// Return that everything went ok
	return true;
}

bool PhysicalSwitch::rewrite_action_set(
		fluid_msg::ActionSet& old_action_set,
		fluid_msg::ActionSet& action_set_with_output,
		fluid_msg::ActionSet& action_set_without_output,
		bool& has_action_with_group,
		VirtualSwitch* virtual_switch) {
	// Loop over all actions
	for( fluid_msg::Action* action : old_action_set.action_set() ) {
		if( action->type() == fluid_msg::of13::OFPAT_OUTPUT ) {
			fluid_msg::of13::OutputAction* output =
				(fluid_msg::of13::OutputAction*) action;

			uint32_t port_no;
			if( output->port()==fluid_msg::of13::OFPP_CONTROLLER ) {
				port_no = output->port();
			}
			else {
				port_no = virtual_switch->get_virtual_port_no(
						features.datapath_id,
						output->port());
			}

			uint32_t group_id = physical_port_to_group_id( port_no );

			action_set_with_output.add_action(
				new fluid_msg::of13::GroupAction(group_id));
		}
		else if( action->type() == fluid_msg::of13::OFPAT_GROUP ) {
			// TODO Rewrite group number
			has_action_with_group = true;
			action_set_with_output.add_action(action->clone());
			action_set_without_output.add_action(action->clone());
		}
		else if( action->type() == fluid_msg::of13::OFPAT_SET_QUEUE ) {
			// Set queue actions are not supported yet
			BOOST_LOG_TRIVIAL(warning) << *this
				<< " received flowmod with set-queue in write-actions";
			return false;
		}
		else {
			// All other actions can be directly passed on to the switch
			action_set_with_output.add_action(action->clone());
			action_set_without_output.add_action(action->clone());
		}
	}

	return true;
}

bool PhysicalSwitch::rewrite_action_list(
		fluid_msg::ActionList& old_action_list,
		fluid_msg::ActionList& new_action_list,
		VirtualSwitch* virtual_switch) {
	for( fluid_msg::Action* action : old_action_list.action_list() ) {
		if( action->type() == fluid_msg::of13::OFPAT_OUTPUT ) {
			// TODO Rewrite output port
			new_action_list.add_action(action->clone());
		}
		else if( action->type() == fluid_msg::of13::OFPAT_GROUP ) {
			// TODO Rewrite group number
			new_action_list.add_action(action->clone());
		}
		else if( action->type() == fluid_msg::of13::OFPAT_SET_QUEUE ) {
			// Set queue actions are not supported yet
			BOOST_LOG_TRIVIAL(warning) << *this
				<< " received flowmod with set-queue in apply-actions";
			return false;
		}
		else {
			// All other actions can be directly passed on to the switch
			new_action_list.add_action(action->clone());
		}
	}

	return true;
}
