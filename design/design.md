This document should summarize the design decisions taken when building the Openflow Hypervisor.
The original goal was to design a Hypervisor that would expose Openflow 1.3 to it's tenants while the network below could consist of Openflow 1.0 and Openflow 1.3 switches.
During design it became apparent that simulating Openflow 1.3 on Openflow 1.0 switches is not feasible.
The proposed solution was to re-route packets from Openflow 1.0 switches to Openflow 1.3 switches and handle the packets there.
Unfortunately doesn't Openflow 1.0 support multiple VLAN tags imposing unacceptable constraints on the Hypervisor.
Therefor is the design below for an Openflow 1.3 Hypervisor.

# Isolation requirements
The Hypervisor needs to prevent tenants from interfering with each other.
This section list the requirements of what needs to be isolated and how to do it.

## Switch features isolation
Each slice wants to be able to use multiple flow tables, group tables, meter tables and queue's.

This is achieved by allocating for each slice an amount of the available resources and rewriting the id's used by that slice.
The first two flow tables are reserved by the hypervisor for handling hypervisor traffic and applying actions to the packets such as rate-limiting certain slices.
Each packet gets a number attached in the metadata field which contains the slice it is in, each flowmod than adds a match to the slice that flowmod belongs to.
This allows each slice to use most tables, a tenant can even use the metadata field only the first 10 bits are reserved for the slice id.

Something similar is done with the group tables and meter tables, the group and meter tables don't have any logical ordering so according to the Openflow protocol a controller may try to use every group table in the 32 bit group id space.
The Hypervisor has to therefor save a map from the virtual id's the slice uses and the real id's in the switch.
When a slice has exhausted the amount of group tables reserved for it send an error message.
The same is used for the meter tables.

The Flood port is simulated by creating a group entry per slice on each switch that creates a packet for each port on the virtual switch.
This can create a lot of traffic over some links that are unnecessary.
For future work a method can be investigated where a flood message is propagated over a spanning tree and duplicated at each switch according to the amount of ports of that virtual switch that are connected to that switch.

## Bandwidth isolation
Each slice should get a guaranteed slice of the bandwidth.

This is achieved by metering a packet before it is forwarded to a slice's flow tables.
This hard limits the amount of packets that each slice can process.
The maximum rate a slice can use is pre-configured and used throughout the entire network.

More fine-grained bandwidth isolation remains future work.
A problem with the current approach is that even if the network has the capability to allow a slice to use more traffic is it not utilized.
During configuration an error might be made reserving more traffic over a link than is actually possible.
This is currently not detected and would allow slices to influence each others traffic.

## Address space isolation
Each slice needs to be able to use the full Ethernet/IP address space without clashing with another slice.

OpenVirtex, an Openflow 1.0 Hypervisor, achieves this by rewriting the source and destination addresses at the edge of the network to an internal representation and rewriting it again before the packet is output again at the network edge.
With Openflow 1.3 this becomes difficult when rewriting packets that are output via a group table (and some other problems) and new features are available which is why a different concept was chosen.

Double VLAN tags was choosen, in this concept VLAN tags are added to each packet containing the slice and the destination.
Openflow switches are not required to allow masking on MPLS tags which would be necessary for efficient routing and PBB tags aren't widely supported.
The first VLAN tag contains the slice/switch combination and the second VLAN tag contains the slice/port combination.
The packet is routed to the switch based on the first tag, 1 hop before it gets to that switch the tag is removed and it is forwarded with only the slice/port VLAN tag.
The target than outputs the packet after removing the VLAN tag over the desired port.
This scheme was choosen to enlarge the amount of bit per slice/switch/port.
We need another bit of information so the switch can differentiate between the two VLAN tags.
Using the VLAN-vid and VLAN-pcp fields we have 15 bits of fields per VLAN tag of which only 12 bits are maskable.
We divide these bits as follows:

VLAN tag 1:
 - 7 bits slice
 - 1 bit is port tag = 0
 - 7 bits switch

Switch VLAN tag: (must be followed by Port VLAN Tag)
 - type = 0 (switch message)
 - 14 bits switch

Port VLAN tag: (followed by tenant packet)
 - type = 1 (port output message)
 - 7 bits port
 - 7 bits slice

Shared link VLAN tag is a port VLAN Tag with port-id=port-max

Topology discovery tag is a port VLAN Tag with slice-id=slice-max, followed by a SwitchVLANTag.
The order is inverted so the packet can be detected by the slice id while still containing all information.

The output action need to be rewritten to add the tag if necessary.
For this purpose is reactively a group table entry created with type indirect for each slice/switch/port combination.
This entry adds the tag(s), sets the values and outputs the packet over the port it needs to go to.
In an apply action instruction the output action can just be changed to the group action without encountering problems but the write-action instruction needs some extra logic.
The action set of a packet can be overwritten by in other flow tables, per type of action only one is allowed in the action set.
If an action set contains an group and an output action only the group action is executed.
The described rewrite would change an output action to a group action, this means it might overwrite a group action set by the tenant in an earlier table.
To solve this problem a bit in the metadata field is used.
When a packet has a group action in the action set the bit is set to 1, when it is removed the bit is set to 0.
Every flow rule that has an output action in the write-action instruction that needs to be rewritten to a group action is duplicated, a version with the output action rewritten that matches on the metadata bit being 1 and a version with the output action removed that matches on the metadata bit being 0.
This adds the constraints to the tenant controller that they cannot match/write on the left-most bit of the metadata field.

## Topology abstraction
Each virtual switch doesn't need to correspond 1:1 to a physical switch.
A tenant might want to abstract away some complexity or the network operator might want to hide some implementation details.

Topology abstraction is allowed by defining each virtual switch as a combination of physical ports, which may be on other switches.
Constraints are imposed such as:
 - Only ports on a switch-switch connection may be shared between multiple virtual switches, if no switch-switch connection is detected the virtual ports stay down
 - If a virtual switch has a port on a switch-switch connection it also has to have a port in that slice on the other side of the connection, if that link is not found the virtual port should stay down
 - The group table type fast-failover cannot be used since the ports that the group might refer to don't necessarily are on the same physical switch

All packets are handled by tenant rules on the switch they arrive in.
If a packet needs to be output on a port on another switch the output action is rewritten to add a VLAN tag containing the switch/port target and output on a switch-switch port with a route to the target.
In the first flowtable packets that need to be forwarded are detected and appropriately forwarded.
If the packet needs to be outputted at this switch is the VLAN tag removed and the packet outputted.

Future work might include optimizing this method for the group table.
If a rule generates a lot of copies of the packet it might be advantageous to apply the group table at a later switch instead of immediately.
Another disadvantage of the current method is that every physical switch has all the rules from all virtual switches that have ports on the physical switch.
This leads to an enormous redundancy amount of rules in each physical switch, only having 1 physical switch with the rules for a virtual switch would lead to more resources (flow/group/meter tables) being available to each virtual switch.

# Flowtable layout
The following section describes the layout of flow rules the Hypervisor.

## Table 0, Hypervisor reserved table

Priority | Purpose | Amount | Cookie | Match | Instructions
---------|---------|--------|--------|-------|-------------
20 | Forward Hypervisor topology discovery packets | 1 | 1 | vlan-type=port, vlan-slice=slice-max | output(controller)
10 | Detect that traffic has arrived over a port with a link | #of ports with links | port | in-port=z | goto-tbl(1)
10 | Act like packets arrived from the controller arrived over a shared link | 1 | port | in-port=controller | goto-tbl(1)
10 | Forward new packet to personal flowtables | # of ports without link in a virtual switch | port | in-port=z | meter(n), write-metadata-group-bit, write-metadata-virtual-switch-bits, goto-tbl(2)
10 | Drop packets that don't belong in a virtual switch | # of ports without link not in a virtual switch | port | in-port=z | drop
 0 | Error detection rule | 1 | 2 | \* | output(controller)

## Table 1, Traffic arrived over port with a link

Priority | Purpose | Amount | Cookie | Match | Instructions
---------|---------|--------|--------|-------|-------------
30 | Forward message over shared link to virtual switch flowtable | # of virtual switches | virtual switch id | in-port=y, vlan-type=port, vlan-port=max-port, vlan-slice=z | pop-vlan, meter(n), write-metadata-group-bit, write-metadata-virtual-switch-bits, goto-tbl(2)
20 | Forward message to other switch | # of switches - 1 | switch id | vlan-type=switch, vlan-switch=z | output(a)
20 | Forward message to other switch that is 1 hop away | # of switches - 1 | switch id | vlan-type=switch, vlan-switch=z | pop-vlan, output(a)
10 | Output preprocessed message over port with link | # of ports | port | vlan-type=port, vlan-port=z | vlan-port=max-port, output(a)
10 | Output preprocessed message over port without link | # of ports | port | vlan-type=port, vlan-port=z | pop-vlan, output(a)
 0 | Error detection rule | 1 | 3 | \* | output(controller)

## Table n | n>=2, tenant tables

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
- | Rewritten flow rules from tenant controller | - | - | -

## Group tables

Purpose | Id | Amount | Mode | Buckets
--------|----|--------|------|--------
Output over to host | Concatenation of virtual-switch-bits, port-bits | # of ports \* # of virtual switches | Indirect | bucket(output(a))
Output over shared link | Concatenation of virtual-switch-bits, port-bits | # of ports \* # of virtual switches | Indirect | bucket(push-vlan, vlan-type=port, vlan-slice=a, vlan-port=max-port, output(b))
Output to port 1 hop away | Concatenation of virtual-switch-bits, port-bits | # of ports \* # of virtual switches | Indirect | bucket(push-vlan, vlan-type=port, vlan-slice=a, vlan-port=b, output(c))
Output to port multiple hop away | Concatenation of virtual-switch-bits, port-bits | # of ports \* # of virtual switches | Indirect | bucket(push-vlan, vlan-type=port, vlan-slice=a, vlan-port=b, group(c))
Add switch VLAN Tag and output packet | 2^13+switch-id | # of switches - 1 | Indirect | bucket(push-vlan, vlan-type=switch, vlan-switch=a, output(b))
Simulate FLOOD output action | 2^26+virtual-switch-id | # of virtual switches | All | bucket(group(x)), etc

## Meter tables
The first n meter tables are reserved where n is the number of slices.
All these have 1 meter band with action drop when the rate comes above the allowed rate for the slice.

# Actions per packet
This section describes per different packet what actions need to be taken on the packet.

## Packets from controller
This section discusses what to do with packets coming from the controller to the switch.

For every packet that get's forward the xid in the openflow header needs to be rewritten to an id unique for that physical switch.
The response from the physical switch, if it get's forwarded back to the controller, needs to be rewritten again.

### Hello
Log and print versions supported by the controller

### Error
Log packet and print error

### EchoRequest
Send an copied EchoRequest to all physical switches that have ports from this virtual switch.
When all their EchoResponses are at the Hypervisor send a EchoResponse back to the controller.

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
Send a GetConfigResponse with the fragmentations_flags & (and operation) and the miss_send_len minimum.

### SetConfig
Drop and do nothing.
Since this affects all packet handling on the switch which may be used by multiple slices is it not sensible to actually pass this on.

### PacketOut
Pick a physical switch this is going to be send to, if there is an output action send it to the switch that has the relevant port on it.
If there is no output action pick a random switch, if there are multiple just use the first one.

The fields to be rewritten are the buffer-id, in-port and the actions (output, group, meter).
The actions are to be rewritten as if they are in an apply-action instruction.
If the buffer-id is used but not available in the switch rewrite map return error Bad Request type Buffer Empty.

If the in-port=controller and there is an output action to the table surround the output to table action with a push-tag, set-field and pop-tag actions.
This is necessary so the Hypervisor reserved table can figure out what slice this packet belongs to.

Send the packet to the switch.

Remove the buffer-id rewrite from the virtual switch.

### FlowMod

If table_id=OFPTT_ALL create clone messages for each table assigned to this slice.
If command=OFPC_DELETE* rewrite the out_port field and out_group field

The flags field can be directly forwarded.

The following rewrite algorithm should be used on apply-action action lists:
```
Scan action list:
  If action is group:
    Rewrite group number
  If action is queue:
    Return error Bad Action with type Unsupported Order
  If action is output:
    If output over FLOOD port:
      Replace with group action to slice flood group
    Else:
      If relevant forwarding group is not created:
        Create groupmod message with actions push-vlan, set-field, output
        Send groupmod message
        Register that group is created
        Send barrierrequest
      Replace with group action that adds correct tag and output over correct link
```

The following algorithm should be used on flowmod messages:
```
Copy packet for each physical switch containing ports of this virtual switch

For each clone packet:
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
        If action is queue:
          Return error Bad Action with type Unsupported Order
        If action is output:
          If output over FLOOD port:
            Replace with group action to slice flood group
            Add matching on metadata 1 and mask 1
            Clone rule with the metadata match on metadata 0 and mask 1 and with this action removed
          Else:
            If relevant forwarding group is not created:
              Create relevant groupmod (look at grouptable examples)
              Send groupmod message
              Register that group is created
              Send barrierrequest
            Replace output action with group action that adds correct tag and output over correct link
            Add matching on metadata 1 and mask 1
            Clone rule with the metadata match on metadata 0 and mask 1 and with this action removed
    If instruction is clear-actions:
      Add write-metadata instruction with metadata 0 and mask 1
    If instruction is write-metadata:
      If left-most x bits in mask is set:
        Drop rule, Return error
      Else:
        Shift mask and metadata x bits to the left

  Scan match fields:
    If match on metadata:
      If left-most x bits in mask is set:
        Drop rule, return error
      Else:
        Shift mask and metadata x bits to the left
        Add slice id metadata
    Add match on slice id metadata

  Send packet(s)
```

For every physical switch can the mapping from virtual to physical be different which is why it needs to be done after cloning the packets.

### GroupMod
Drop if fast-failover

The apply-action algorithm should be applied to each bucket individually.

Apply the following algorithm:
```
If type=fast-failover:
  Send error Group Mod Failed with type Bad Type.
  Done

For each physical switch this virtual switch depends on:
  If type=delete:
    If group-id=all:
      Clone packet for each group-id in use by this virtual switch in physical switch group map
      Rewrite group-id's
      Send packets
      Remove group-id's from physical switch map
    Else:
      Rewrite group-id
      Send packet
      Remove group-id from physical switch map
  If type=add:
    Scan buckets:
      Use apply-action algorithm on the bucket action list
    Reserve new group-id in physical switch group-id map
    Rewrite group-id
    Send packet
  If type=modify:
    If group-id is not in physical switch group-id map:
      Send error Group Mod Failed and type Unknown Group
      Done
    Rewrite group-id
    Scan buckets:
      Use apply-action algorithm on the bucket action list
    Send packet
```

### MeterMod
Use the same algorithm for GroupMod, instead of fast-failover stop meter being placed on the controller of the slow datapath.

```
If meter-id=controller or meter-id=slowpath:
  Send error Meter Mod Failed with type Invalid Meter
  Done

For each physical switch this virtual switch depends on:
  If command=delete:
    If meter-id=all:
      Clone packet for each meter-id in use by this virtual switch in physical switch group map
      Rewrite meter-id's
      Send packets
      Remove meter-id's from physical switch map
    Else:
      Rewrite meter-id
      Send packet
      Remove meter-id from physical switch map
  If command=add:
    Reserve new meter-id in physical switch meter-id map
    Rewrite meter-id
    Send packet
  If type=modify:
    If meter-id is not in physical switch meter-id map:
      Send error Meter Mod Failed and type Unknown Meter
    Rewrite meter-id
    Send packet
```

### PortMod
Send error Port Mod Failed with type EPerm.

### TableMod
This message is deprecated in openflow 1.3, don't do anything.

### MultipartRequest
Support only the bare minimum so an experiment can be done where the hypervisor runs on top of itself.
This would at least involve the PortDescription message.
Otherwise return a Bad Request error with type Bad Multipart.
This indicates that this type of multipart message is not supported.

Providing statistics is currently out of scope for this project.
Statistic messages could be rewritten, passed to the switches and aggregated to provide this functionality.
Port statistics can be tricky since on shared links the traffic caused by each slices would need to be isolated.
This could be done by remembering which flows cause packets to be forwarded to a port and query their statistics instead and aggregate those.

### QueueGetConfigRequest
Return an empty QueueGetConfigResponse message.

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

### Error
The error should be forwarded to the relevant tenant controller.

Lookup via the xid map in the physical switch structure what virtual switch this was initiated from. Rewrite the xid and forward to the tenant.

Optionally log and print the error.

TODO The error message contains part of openflow message that generated it. This is currently just forwarded without any rewrite happening.

### PacketIn
Use the following algorithm:
```
If table-id=0 or table-id=1:
  If cookie=1:
    Packet from topology discovery, extract what switch it is from, mark link live, reset liveness timer
  Else:
    Log packet, print error
Else:
  Figure out what slice generated this error via metadata oxm

  Rewrite table-id in packet

  Scan match fields:
    If match=metadata:
      Shift metadata value right 11 bits (no need to remove if metadata is now 0, optional OXM TLV can be included)
    If match=in-port:
      Rewrite in port to virtual port
    If match=physical-in-port:
      Remove from match

  If buffer-id!=no-buffer:
    Check all virtual switches depending on this physical switch:
      Remove this buffer-id from there buffer-id rewrite map
    Save buffer-id in virtual switch rewrite map

  Send packet to tenant controller
```

TODO Does it happen that a packet for a tenant generates a TTL PacketIn when arriving at the switch instead of on the dec-ttl actions? In that case the slice/tenant needs to be discovered looking at the VLAN tags or the port.

The buffer-id maps do not need to be maintained since if the buffer doesn't exist anymore on the switch is the error message just forwarded back to the tenant.
If a buffer-id would be re-used by a switch is the entry deleted from all virtual switches and does the PacketOut return an error from the Hypervisor.

### FlowRemoved
Some rules from the tenant become 2 rules in the physical switch (group action in the write-action instruction).
Make sure that stays consistent.

If the flow came from table 1 or 2 log error, otherwise figure out what slice generated this message via metadata oxm and forward to a random virtual switch that has a port on this physical switch.

Only forward if the controller Async request filter says the controller wants to receive these packets.

### PortStatus
Update set of ports appropriately (add/remove/modify)

Figure out what virtual ports are affected, send those tenants appropriate PortStatus messages.

Only forward if the controller Async request filter says the controller wants to receive these packets.

## Packets initiated from Hypervisor
All the packets that the Hypervisor initiates.

### Session setup and maintenance packets
At the start of a connection send a hello packet that indicates the hypervisor only supports Openflow 1.3.
Afterwards periodically send EchoRequests to each controller and switch.
The echo requests

### Topology discovery packets
The topology discovery packets are as tiny as possible packets with a vlan-tag.
Periodically over every port is a packet send with slice 255, the switch bits set to the switch it originates from and the port bits set to the port it originates from.
When this packet is received by a switch is it send to the controller so the controller can defer that a link exists.

Topology discovery is run per port every second.
To lower the load on each control channel are the packets spaced out over the period.
The algorithm executed is:
```
global counter = 0;

For each physical switch:
  Repeat:
    Send packet over port counter

    counter = (counter+1)%max(ports)

    wait period/max(ports)
```

This is just a barebone version of topology discovery.
Future work could be signing the packets so spoofing fake links would be harder or to reduce the load even more could ports that currently have no link receive less packets.

### Changed topology group modifications
If the topology is changed as detected by topology discovery does the routing need to be changed.
All the routing between switches done by the Hypervisor are actually forwarded via the group table.
To update the routes the following needs to happen:
```
Find all-to-all network routes with floyd-warshall.

For each physical switch:
  For each created forward group entry:
    Extract target switch from id switch bits
    If the new route is different than the route stored:
      Create group mod modify message
      Send packet
      Store new route
```

# Data necessary
This section lists what data needs to be saved in the Hypervisor to function.

## Globally
 - Bidirectional map from id to physical switch
 - Map to lookup physical switch via datapath-id

## Per slice
 - Slice id
 - Maximum rate

## Per virtual switch
 - datapath-id (made up, unique)
 - Map of actual packet buffers to virtual buffers and vice-versa, (dpid,buffer-no) <-> virtual-buffer-no
 - packet-in-mask (=3)
 - port-status-mask (=7)
 - flow-removed-mask (=15)
 - If this switch received an EchoReply on the last EchoRequest
 - All physical switches this virtual switch depends on

## Per physical switch
 - n-buffers
 - n-tables
 - capabilities (only IP_REASM is currently used)
 - fragmentation-flags (ask via GetConfig)
 - miss-send-len (ask via GetConfig)
 - Set of ports (count port status)
 - Internal id (for routing)
 - If this switch received an EchoReply on the last EchoRequest
 - Map of created forward group entries (the group-id's of groups that are created that forward traffic to an output port while adding a vlan-tag(s)) to the output port they forward to, group-id -> output-port
 - Map of group-id's to virtual group-id's, group-id <-> (virtual-dpid,group-id)
 - Map of meter-id's to virtual meter-id's, meter-id <-> (virtual-dpid,meter-id)
 - Map of xid's so responses can be properly forwarded, xid <-> (virtual-switch,xid)
 - For all other physical switches, on what port to route packets
 - All virtual switches depending on this physical switch

# Events to handle
This section discusses some event that may happen that aren't directly openflow messages.

## New switch connection
 - Send Hello message
 - Send FeatureRequest message
 - Send GetConfig message
 - Send updated PortStatus messages for all virtual ports on this switch that are now online

## Lost switch connection
 - Send port-down messages for all virtual ports on this switch
 - Rerun routing algorithm, update all routes

## New controller connection
 - Send Hello message
 - Send PortStatus messages for all virtual ports with their current state

## Lost controller connection
TODO Remove the rules pushed to physical switches?

## Link lost/found detected via topology discovery
```
Compute new all-to-all routing via floyd-warshall
For each virtual switch:
  Check if all depended on physical switch is online and can reach any specific switch
  If so:
    For each depended on physical switch:
      Add slice specific forwarding rules
    Make virtual switch live
  Else:
    Make virtual switch down
```

# Notes
Let all xid live in the system for a while so you know to what tenant to forward an error when you get 1

Some modification messages generate multiple modification messages to the switches.
If an error exists in those messages how to make sure the tenant only receives 1 error message?
What to do with messages that get accepted by only a subset of all the switches?

Need a way to easily lookup physical switches by datapath id

Abilities needed:
 - Lookup if all virtual switches that depend on a physical switch with a certain dpid, dpid -> {virtual-switch, virtual-switch, ...}

No personal tables, just match on metadata

A virtual switch only comes online after all physical switches it depends on are online and can route to each other, it goes offline again if a physical switch or route disappears.

TODO Remove this section
