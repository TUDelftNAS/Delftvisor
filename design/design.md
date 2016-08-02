This document should summarize the design decisions taken when building the Openflow Hypervisor. The original goal was to design a Hypervisor that would expose Openflow 1.3 to it's tenants while the network below could consist of Openflow 1.0 and Openflow 1.3 switches. During design it became apparent that simulating Openflow 1.3 on Openflow 1.0 switches is not feasible. The proposed solution was to re-route packets from Openflow 1.0 switches to Openflow 1.3 switches and handle the packets there. Unfortunately doesn't Openflow 1.0 support multiple VLAN tags imposing unacceptable constraints on the Hypervisor. Therefor is the design below for an Openflow 1.3 Hypervisor.

# Isolation requirements
The Hypervisor needs to prevent tenants from interfering with each other. This section list the requirements of what needs to be isolated and how to do it.

## Switch features isolation
Each slice wants to be able to use multiple flow tables, group tables, meter tables and queue's.

This is achieved by allocating for each switch an amount of the available resources and rewriting the id's used by that switch. The first flow table is than used to forward packets to the flow tables it is meant to go into.

For example if slice 3 used the flow tables 10 to 20 and a packet arrives for that slice is that packet sent to table 20. When slice 3 tries to set a flow rule in table 1 it is actually entered in table 10. If any of the flow rules set by table 3 have an goto-table instruction the table number is rewritten, so table 1 becomes 11 and so forth.

Something similar is done with the group tables and meter tables, the group and meter tables don't have any logical ordering so according to the Openflow protocol a controller may try to use every group table in the 32 bit group id space. The Hypervisor has to therefor save a map from the virtual id's the slice uses and the real id's in the switch. When a slice has exhausted the amount of group tables reserved for it sent an error message. The same is used for the meter tables.

## Bandwidth isolation
Each slice should get a guaranteed slice of the bandwidth.

This is achieved by metering a packet before it is forwarded to a slice's flow tables. This hard limits the amount of packets that each slice can process. The maximum rate a slice can use is pre-configured and used throughout the entire network.

More fine-grained bandwidth isolation remains future work. A problem with the current approach is that even if the network has the capability to allow a slice to use more traffic is it not utilized. During configuration an error might be made reserving more traffic over a link than is actually possible. This is currently not detected and would allow slices to influence each others traffic.

## Address space isolation
Each slice needs to be able to use the full Ethernet/IP address space without clashing with another slice.

OpenVirtex, an Openflow 1.0 Hypervisor, achieves this by rewriting the source and destination addresses at the edge of the network to an internal representation and rewriting it again before the packet is output again at the network edge. With Openflow 1.3 this becomes difficult when rewriting packets that are output via a group table (and some other problems) which is why a different concept was chosen.

PBB (Provider Backbone Bridging, also called mac-in-mac) was chosen because it allows for 120 bits that support masking. Openflow switches are not required to allow masking on MPLS tags which would be necessary for efficient routing and VLAN tags only allow for 15 bits of maskable information which wasn't enough to contain the routing information for a sizable network.

The output action need to be rewritten to add the tag if necessary. For this purpose is reactively a group table entry created with type indirect for each slice/switch/port combination. This entry adds the PBB tag, sets the values and outputs the packet over the port it needs to go to. In an apply action instruction the output action can just be changed to the group action without encountering problems but the write-action instruction needs some extra logic. The action set of a packet can be overwritten by in other flow tables, per type of action only one is allowed in the action set. If an action set contains an group and an output action only the group action is executed. The described rewrite would change an output action to a group action, this means it might overwrite a group action set by the tenant in an earlier table. To solve this problem a bit in the metadata field is used. When a packet has a group action in the action set the bit is set to 1, when it is removed the bit is set to 0. Every flow rule that has an output action in the write-action instruction that needs to be rewritten to a group action is duplicated, a version with the output action rewritten that matches on the metadata bit being 1 and a version with the output action removed that matches on the metadata bit being 0. This adds the constraints to the tenant controller that they cannot match/write on the left-most bit of the metadata field.

PBB allows for an extra pair of source/destination mac addresses in a packet and also adds a 24 bit I-SID of which all bits are maskable. This allows for 120 bits of extra information per packet. Unfortunately are the tags applied at the end by a dedicated group table and the group table id's are only 32 bit. Since we also need to reserve some group id's for the tenants only 31 bits are used. Another problem comes with the amount of flow tables, the flow table id is 8 bits and per slice we need at least 1 flow table limiting the number of slices to a maximum of 254 (8 bits). These would be split up using:
 - 8 bit slice id
 - 12 bit switch id
 - 11 bit port id

## Topology abstraction
Each virtual switch doesn't need to correspond 1:1 to a physical switch. A tenant might want to abstract away some complexity or the network operator might want to hide some implementation details.

Topology abstraction is allowed by defining each virtual switch as a combination of physical ports, which may be on other switches. Constraints are imposed such as:
 - Only ports on a switch-switch connection may be shared between multiple virtual switches, if no switch-switch connection is detected the virtual ports stay down.
 - If a virtual switch has a port on a switch-switch connection it also has to have a port in that slice on the other side of the connection, if that link is not found the virtual port should stay down
 - The group table type fast-failover cannot be used since the ports that the group might refer to don't necessarily are on the same physical switch.

All packets are handled by tenant rules on the switch they arrive in. If a packet needs to be output on a port on another switch the output action is rewritten to add a BPP tag containing the switch/port target and output on a switch-switch port with a route to the target. In the first flowtable packets that need to be forwarded are detected and appropriately forwarded. If the packet needs to be outputted at this switch is the PBB tag removed and the packet outputted.

Future work might include optimizing this method for the group table. If a rule generates a lot of copies of the packet it might be advantageous to apply the group table at a later switch instead of immediately.

# Flowtable layout
The following section describes the layout of flow rules the Hypervisor.

## Table 0, Hypervisor reserved table:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
70 | Forward Hypervisor topology discovery packets, cookie=1 | 1 | eth-src=x, eth-dst=y | output(controller)
60 | Forward packet-out from tenant to personal flowtables | # of slices | in-port=controller, pbb-slice-bits=x | meter(n), write-metadata(0&1), pop-pbb, goto-tbl(n\*k+1)
50 | Forward inter-virtual-switch traffic | # of ports with links \* (# of switches-1) | in-port=x, pbb-switch-bits=y | output(z)
40 | Output already processed packet over port with link | # of slices \* # of ports with link \* # of ports with link | in-port=x, pbb-slice-bits=y pbb-switch-bits=z, pbb-port-bits=w | meter(n), output(a)
30 | Output already processed packet over port without link | # of ports with link \* # of ports without link | in-port=x, pbb-switch-bits=y, pbb-port-bits=z | pop-pbb, output(a)
20 | Detect erroneous packets on switch links, cookie=2 | # of ports with link | in-port=x | output(controller)
10 | Forward new packet to personal flowtables | # of ports without link | in-port=x | meter(n), write-metadata(0&1), goto-tbl(n\*k+1)
 0 | Error detection rule, cookie=3 | 1 | * | output(controller)

## Table n | n>0, tenant tables:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
- | Rewritten flow rules from tenant controller | - | - | -

## Group tables

Purpose | Id | Amount | Mode | Buckets
--------|----|--------|------|--------
Add outgoing PBB tag | Concatenation of slice-bits, switch-bits, port-bits | # of slices \* # of ports in network not on this switch \* # of ports | Indirect | bucket(push-pbb, pbb-slice-bits=a, pbb-switch-bits=b, pbb-port-bits=c, output(d))
Simulate FLOOD output action | 2^31+slice-id | # of slices | All | bucket(output(x)), bucket(group(x)), etc

## Meter tables:
The first n meter tables are reserved where n is the number of slices.
All these have 1 meter band with action drop when the rate comes above the allowed rate for the slice.

# Actions per packet
This section describes per different packet what actions need to be taken on the packet.

## Packets from controller
This section discusses what to do with packets coming from the controller to the switch.

For every packet that get's forward the xid in the openflow header needs to be rewritten to an id unique for that physical switch.
The response from the physical switch, if it get's forwarded back to the controller, needs to be rewritten again.

### Hello
Sent back an Hello message indicating the Hypervisor only support openflow 1.3.

### EchoRequest
Sent an copied EchoRequest to all physical switches that have ports from this virtual switch.
When all their EchoResponses are at the Hypervisor sent a EchoResponse back to the controller.

If any response comes back with an error forward that error, if an response doesn't come back also don't respond.

### BarrierRequest
See EchoRequest, do the same thing.

### Experimenter
Return an error with type BadRequest and code BadExperimenterType.

### FeatureRequest
Return an FeatureReply with:
 - datapath_id  = Virtual switch datapath id
 - n_buffers    = minimum n_buffers of underlying physical switches
 - n_tables     = Amount of tables reserved for this slice
 - auxiliary_id = 0, indicates this is the main connection
 - capabilities = 0, Statistics are currently not in the scope of this project. The IP_REASM should be set if all underlying switches implement it.

### GetConfigRequest
Send a GetConfigResponse with the fragmentations_flags & and the miss_send_len minimum.

### SetConfig
Drop and do nothing. Since this affects all packet handling on the switch which may be used by multiple slices is it not sensible to actually pass this on.

### PacketOut
Pick a physical switch this is going to be sent to, if there is an output action sent it to the switch that has the relevant port on it. If there is no output action pick a random switch, if there are multiple just use the first one.

The fields to be rewritten are the buffer_id, in_port and the actions (output, group, meter). The actions are to be rewritten as if they are in an apply-action instruction.

If the in-port=controller and there is an output action to the table surround the output to table action with a push-tag, set-field and pop-tag actions. This is necessary so the Hypervisor reserved table can figure out what slice this packet belongs to.

Sent the packet to the switch.

Remove the buffer_id rewrite from the virtual switch.

TODO Use VLAN tag instead of PBB tag since it's smaller?

### FlowMod

If table_id=OFPTT_ALL create clone messages for each table assigned to this slice.
If command=OFPC_DELETE* rewrite the out_port field and out_group field

The flags field can be directly forwarded.
TODO Should the cookie field be rewritten?

The following rewrite algorithm should be used on apply-action action lists:
```
Scan action list:
  If action is group:
    Rewrite group number
  If action is output:
    If output to port on this switch:
      Rewrite output port number
    If output over FLOOD port:
      Replace with group action to slice flood group
    If output to port not on this switch:
      Replace with group action that adds correct tag and output over correct link
```

The following algorithm should be used on flowmod messages:
```
Scan instruction list:
  If instruction is goto-tbl:
    Rewrite goto-tbl number
  If instruction is meter:
    Rewrite meter number
  If instruction is apply-action:
    Use apply-action list algorithm
  If instruction is write-action:
    Scan action list:
      If action is group:
        Rewrite group number
        Add instruction write-metadata with data 1 and mask 1
      If action is output:
        If output to port on this switch:
          Rewrite output port number
        If output over FLOOD port:
          Replace with group action to slice flood group
          Add matching on metadata 1 and mask 1
          Clone rule with the metadata match on metadata 0 and mask 1 and with this action removed
        If output to port not on this switch:
          Replace with group action that adds correct tag and output over correct link
          Add matching on metadata 1 and mask 1
          Clone rule with the metadata match on metadata 0 and mask 1 and with this action removed
  If instruction is clear-actions:
    Add write-metadata instruction with metadata 0 and mask 1
  If instruction is write-metadata:
    If left-most bit in mask is set:
      Drop rule, Return error
    Else:
      Shift mask and metadata 1 bit to the left
Scan match fields:
  If match on metadata:
    If left-most bit in mask is set:
      Drop rule, return error
    Else:
      Shift mask and metadata 1 bit to the left
```

### GroupMod
Drop if fast-failover

The apply-action algorithm should be applied to each bucket individually.

TODO How to do Modify/Delete matching

### MeterMod
The meter identifier needs to be rewritten to a meter id allocated to the slice.
If the meter id was out of the allowed range of meters for the slice the hypervisor will sent back an Error with type MeterModFailed and code OutofMeters.
If the meter id is OFMP_ALL simulate it by deleting all meters in the slice range.
The packet can then be forwarded.

TODO simulating OFMP_ALL is slow, dealing with the response/failure messages of that seems difficult and the standard doesn't say you need to use adjacent meter id's.

### PortMod

TODO Don't allow. Ports may be used by multiple slices.

### TableMod
This message is deprecated in openflow 1.3, don't do anything.

### MultipartRequest
Return a Bad Request error with type Bad Multipart. This indicates that this type of multipart message is not supported.

Providing statistics is currently out of scope for this project. Statistic messages could be rewritten, passed to the switches and aggregated to provide this functionality. Port statistics can be tricky since on shared links the traffic caused by each slices would need to be isolated. This could be done by remembering which flows cause packets to be forwarded to a port and query their statistics instead and aggregate those.

### QueueGetConfigRequest

### RoleRequest
Send an error message with type RoleRequestFailed and code Unsupported.

### GetAsyncRequest
Retreive the setting from the virtual-switch structure and send a GetAsyncResponse message with these settings in them.

### SetAsync
Save the new settings in the virtual-switch structure.

Technically should these settings be saved per connection instead of per switch.

## Packets from a switch
This section discusses the actions to perform on packets that are received unsollicited from a switch.
The packets EchoResponse, FeatureResponse, GetConfigResponse, QueueGetConfigResponse are already discussed in the previous section.

### PacketIn
Determine if this is caused by an address that needs to be rewritten.
If table_id=0 & cookie=1 the packet comes from topology discovery.
  Mark the link as live, reset liveness timer, drop packet.
Else
  Log packet, show error

Figure out what slice this packet comes from.
Rewite table_id to slice table.
Only forward if the controller Async request filter says the controller wants to receive these packets.

### FlowRemoved

Only forward if the controller Async request filter says the controller wants to receive these packets.

### PortStatus

Only forward if the controller Async request filter says the controller wants to receive these packets.

## Packets initiated from switch
All the packets that the switch initiates.

### Session setup and maintenance packets

Periodically sent EchoRequests to each controller.

### Topology discovery PacketOut
Periodically sent PacketOut packets to every physical port to check if a link (still) is present.

The topology discovery packets are LLDP packets sent in a reserved slice with id 0.

# Data necessary
This section lists what data needs to be saved in the Hypervisor to function.

## Per slice

## Per virtual switch
 - datapath_id (made up, unique)
 - Map of actual packet buffers to virtual buffers and vice-versa, (dpid,buffer_no) <-> virtual_buffer_no
 - packet_in_mask (=3)
 - port_status_mask (=7)
 - flow_removed_mask (=

## Per switch
 - n_buffers
 - n_tables
 - capabilities (only IP_REASM is currently used)
 - fragmentation_flags (ask via GetConfig)
 - miss_send_len (ask via GetConfig)
 - Map of group_id's to virtual group_id's, group_id <-> (virtual_dpid,group_id)
 - For all other switches, on what port to route packets

## Async request filter
The asynchronous request filter saves per controller what unsollicited messages they want to receive.

# Events to handle
This section discusses some event that may happen that aren't directly openflow messages.

## New switch connection
 - Sent port-up messages for all virtual ports on this switch
 - Rerun routing algorithm, update all routes

## Lost switch connection
 - Sent port-down messages for all virtual ports on this switch
 - Rerun routing algorithm, update all routes

## New controller connection

## Lost controller connection

## New link found via topology discovery

## Link lost detected via topology discovery
 - Sent port-down messages for all virtual ports using either link port
 - Rerun routing algorithm, update all routes

# Notes
Let all xid live in the system for a while so you know to what tenant to forward an error when you get 1

TODO Remove this section
