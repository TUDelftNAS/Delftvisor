#include "tag.hpp"

VLANTag::VLANTag() :
	tag(0),
	mask(0) {
}

VLANTag VLANTag::create_from_raw(uint16_t raw) {
	// Remove the 12th bit, this is the CFI bit and is always 1
	raw = (raw&make_mask(12)) |
		((raw>>1)&(make_mask(3)<<12));

	return VLANTag(raw);
}

uint16_t VLANTag::make_raw() const {
	return (tag&make_mask(12)) |
		(1<<12) |
		((tag&(make_mask(3)<<12))<<1);
}

void VLANTag::set_is_port_tag(int is_port_tag_bit) {
	tag  |= (is_port_tag_bit&1)             << is_port_tag_offset;
	mask |= make_mask(num_is_port_tag_bits) << is_port_tag_offset;
}
void VLANTag::set_slice(int slice_id) {
	tag  |= (slice_id&make_mask(num_slice_bits)) << slice_offset;
	mask |= make_mask(num_slice_bits)            << slice_offset;
}
void VLANTag::set_switch(int switch_id) {
	tag  |= (switch_id&make_mask(num_switch_bits)) << switch_offset;
	mask |= make_mask(num_switch_bits)             << switch_offset;
}

int VLANTag::get_is_port_tag() const {
	return (tag>>is_port_tag_offset)&1;
}
int VLANTag::get_slice() const {
	return (tag>>slice_offset)&make_mask(num_slice_bits);
}
int VLANTag::get_switch() const {
	return (tag>>switch_offset)&make_mask(num_switch_bits);
}

void VLANTag::add_to_match(fluid_msg::of13::FlowMod& flowmod) const {
	uint32_t vid_tag  = tag        & make_mask(12);
	uint32_t vid_mask = mask       & make_mask(12);
	uint32_t pcp_tag  = (tag>>12)  & make_mask( 3);
	uint32_t pcp_mask = (mask>>12) & make_mask( 3);

	// Always add the vid tag so OFPVID_PRESENT bit is
	// always set which is mandatory for PCP tag matching
	flowmod.add_oxm_field(
		new fluid_msg::of13::VLANVid(
			vid_tag  | fluid_msg::of13::OFPVID_PRESENT,
			vid_mask | fluid_msg::of13::OFPVID_PRESENT));

	if( pcp_mask != 0 ) {
		// TODO The PCP bits aren't maskable, throw an error
		// if the pcp_mask isn't make_mask(3)
		flowmod.add_oxm_field(
			new fluid_msg::of13::VLANPcp(
				pcp_tag));
	}
}

template<class ActionSet>
void VLANTag::add_to_actions(ActionSet& action_set) const {
	uint32_t vid_tag = tag       & make_mask(12);
	uint32_t pcp_tag = (tag>>12) & make_mask( 3);

	action_set.add_action(
		new fluid_msg::of13::SetFieldAction(
			new fluid_msg::of13::VLANVid(
				vid_tag | fluid_msg::of13::OFPVID_PRESENT)));
	action_set.add_action(
		new fluid_msg::of13::SetFieldAction(
			new fluid_msg::of13::VLANPcp(pcp_tag)));
}

namespace{
// Force versions of add_to_actions<> using write or apply
// actions to be available during linking
void ugly_hack() {
	fluid_msg::of13::WriteActions w;
	fluid_msg::of13::ApplyActions a;
	VLANTag t;
	t.add_to_actions(w);
	t.add_to_actions(a);
}
}

MetadataTag::MetadataTag() :
	tag(0),
	mask(0) {
}

void MetadataTag::set_group(int group_id) {
	tag  |= (group_id&1);
	mask |= 1;
}
void MetadataTag::set_slice(int slice_id) {
	tag  |= (slice_id&make_mask(num_slice_bits)) << 1;
	mask |= make_mask(num_slice_bits)            << 1;
}

bool MetadataTag::get_group() const {
	return tag&1;
}
int MetadataTag::get_slice() const {
	return (tag>>1) & make_mask(num_slice_bits);
}

bool MetadataTag::add_to_match(fluid_msg::of13::FlowMod& flowmod) const {
	fluid_msg::of13::OXMTLV* oxm = flowmod.get_oxm_field(
			fluid_msg::of13::OFPXMT_OFB_METADATA);
	// If there is an existing match on metadata
	if( oxm != nullptr ) {
		fluid_msg::of13::Metadata* existing_metadata =
			(fluid_msg::of13::Metadata*) oxm;

		// If the existing value is not masked can new
		// data never be added
		if( !existing_metadata->has_mask() ) {
			return false;
		}

		// TODO Continue
	}
	return true;
}

bool MetadataTag::add_to_instruction(fluid_msg::of13::FlowMod& flowmod) const {
	flowmod.add_instruction(
		new fluid_msg::of13::WriteMetadata(
			tag,
			mask));
	return true;
}
