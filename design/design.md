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

This is achieved by allocating for each switch an amount of the available resources and rewriting the id's used by that switch.
The first flow table is than used to forward packets to the flow tables it is meant to go into.

For example if slice 3 used the flow tables 10 to 20 and a packet arrives for that slice is that packet send to table 20.
When slice 3 tries to set a flow rule in table 1 it is actually entered in table 10.
If any of the flow rules set by table 3 have an goto-table instruction the table number is rewritten, so table 1 becomes 11 and so forth.

Something similar is done with the group tables and meter tables, the group and meter tables don't have any logical ordering so according to the Openflow protocol a controller may try to use every group table in the 32 bit group id space.
The Hypervisor has to therefor save a map from the virtual id's the slice uses and the real id's in the switch.
When a slice has exhausted the amount of group tables reserved for it send an error message.
The same is used for the meter tables.

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
With Openflow 1.3 this becomes difficult when rewriting packets that are output via a group table (and some other problems) which is why a different concept was chosen.

PBB (Provider Backbone Bridging, also called mac-in-mac) was chosen because it allows for 120 bits that support masking.
Openflow switches are not required to allow masking on MPLS tags which would be necessary for efficient routing and VLAN tags only allow for 15 bits of maskable information which wasn't enough to contain the routing information for a sizable network.

The output action need to be rewritten to add the tag if necessary.
For this purpose is reactively a group table entry created with type indirect for each slice/switch/port combination.
This entry adds the PBB tag, sets the values and outputs the packet over the port it needs to go to.
In an apply action instruction the output action can just be changed to the group action without encountering problems but the write-action instruction needs some extra logic.
The action set of a packet can be overwritten by in other flow tables, per type of action only one is allowed in the action set.
If an action set contains an group and an output action only the group action is executed.
The described rewrite would change an output action to a group action, this means it might overwrite a group action set by the tenant in an earlier table.
To solve this problem a bit in the metadata field is used.
When a packet has a group action in the action set the bit is set to 1, when it is removed the bit is set to 0.
Every flow rule that has an output action in the write-action instruction that needs to be rewritten to a group action is duplicated, a version with the output action rewritten that matches on the metadata bit being 1 and a version with the output action removed that matches on the metadata bit being 0.
This adds the constraints to the tenant controller that they cannot match/write on the left-most bit of the metadata field.

PBB allows for an extra pair of source/destination mac addresses in a packet and also adds a 24 bit I-SID of which all bits are maskable.
This allows for 120 bits of extra information per packet.
Unfortunately are the tags applied at the end by a dedicated group table and the group table id's are only 32 bit.
Since we also need to reserve some group id's for the tenants only 31 bits are used.
Another problem comes with the amount of flow tables, the flow table id is 8 bits and per slice we need at least 1 flow table limiting the number of slices to a maximum of 254 (8 bits).
These would be split up using:
 - 8 bit slice id
 - 12 bit switch id
 - 11 bit port id

## Topology abstraction
Each virtual switch doesn't need to correspond 1:1 to a physical switch.
A tenant might want to abstract away some complexity or the network operator might want to hide some implementation details.

Topology abstraction is allowed by defining each virtual switch as a combination of physical ports, which may be on other switches.
Constraints are imposed such as:
 - Only ports on a switch-switch connection may be shared between multiple virtual switches, if no switch-switch connection is detected the virtual ports stay down
 - If a virtual switch has a port on a switch-switch connection it also has to have a port in that slice on the other side of the connection, if that link is not found the virtual port should stay down
 - The group table type fast-failover cannot be used since the ports that the group might refer to don't necessarily are on the same physical switch

All packets are handled by tenant rules on the switch they arrive in.
If a packet needs to be output on a port on another switch the output action is rewritten to add a BPP tag containing the switch/port target and output on a switch-switch port with a route to the target.
In the first flowtable packets that need to be forwarded are detected and appropriately forwarded.
If the packet needs to be outputted at this switch is the PBB tag removed and the packet outputted.

Future work might include optimizing this method for the group table.
If a rule generates a lot of copies of the packet it might be advantageous to apply the group table at a later switch instead of immediately.
Another disadvantage of the current method is that every physical switch has all the rules from all virtual switches that have ports on the physical switch.
This leads to an enormous redundancy amount of rules in each physical switch, only having 1 physical switch with the rules for a virtual switch would lead to more resources (flow/group/meter tables) being available to each virtual switch.

# Flowtable layout
The following section describes the layout of flow rules the Hypervisor.

## Table 0, Hypervisor reserved table:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
70 | Forward Hypervisor topology discovery packets, cookie=1 | 1 | pbb-slice-bits=255 | output(controller)
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
Add outgoing PBB tag, added reactively | Concatenation of slice-bits, switch-bits, port-bits | variable, but at most # of slices \* # of ports in network not on this switch \* # of ports | Indirect | bucket(push-pbb, pbb-slice-bits=a, pbb-switch-bits=b, pbb-port-bits=c, output(d))
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

TODO Use VLAN tag instead of PBB tag since it's smaller?

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
    If output to port on this switch:
      Rewrite output port number
    If output over FLOOD port:
      Replace with group action to slice flood group
    If output to port not on this switch:
      If relevant forwarding group is not created:
        Create groupmod message with actions push-pbb, set-field, output
        Send groupmod message
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
          If output to port on this switch:
            Rewrite output port number
          If output over FLOOD port:
            Replace with group action to slice flood group
            Add matching on metadata 1 and mask 1
            Clone rule with the metadata match on metadata 0 and mask 1 and with this action removed
          If output to port not on this switch:
            If relevant forwarding group is not created:
              Create groupmod message with actions push-pbb, set-field, output
              Send groupmod message
              Send barrierrequest
            Replace output action with group action that adds correct tag and output over correct link
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
Return a Bad Request error with type Bad Multipart.
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
If table-id=0:
  If eth-src=x & eth-dst=y:
    Packet from topology discovery, extract what switch it is from, mark link live, reset liveness timer
  Else:
    Log packet, print error
Else:
  Figure out what slice generated this error via table-id

  Rewrite table-id in packet

  Scan match fields:
    If match=metadata:
      Shift metadata value right (no need to remove if metadata is now 0, optional OXM TLV can be included)
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

TODO Does it happen that a packet for a tenant generates a TTL PacketIn when arriving at the switch instead of on the dec-ttl actions? In that case the slice/tenant needs to be discovered looking the the PBB tag.

The buffer-id maps do not need to be maintained since if the buffer doesn't exist anymore on the switch is the error message just forwarded back to the tenant.
If a buffer-id would be re-used by a switch is the entry deleted from all virtual switches and does the PacketOut return an error from the Hypervisor.

### FlowRemoved

Some rules from the tenant become 2 rules in the physical switch (group action in the write-action instruction).
Make sure that stays consistent.

Only forward if the controller Async request filter says the controller wants to receive these packets.

### PortStatus

Figure out what virtual ports are affected.

Only forward if the controller Async request filter says the controller wants to receive these packets.

## Packets initiated from Hypervisor
All the packets that the Hypervisor initiates.

### Session setup and maintenance packets
At the start of a connection send a hello packet that indicates the hypervisor only supports Openflow 1.3.
Afterwards periodically send EchoRequests to each controller and switch.
The echo requests

### Topology discovery packets
The topology discovery packets are as tiny as possible packets with a pbb-tag.
Periodically over every port is a packet send with slice 255, the switch bits set to the switch it originates from and the port bits set to the port it originates from.
When this packet is received by a switch is it send to the controller so the controller can defer that a link exists.

Topology discovery is run per port every second.
To lower the load on each control channel are the packets spaced out over the period.
The algorithm executed is:
```
global counter = 0;

Repeat:
  For each physical switch:
    If port counter exists:
      Send packet over port

  counter += 1
  if counter=max(ports)
    counter = 0

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
 - Internal id (for routing)
 - If this switch received an EchoReply on the last EchoRequest
 - Map of created forward group entries (the group-id's of groups that are created that forward traffic to an output port while adding a pbb-tag) to the output port they forward to, group-id -> output-port
 - Map of group-id's to virtual group-id's, group-id <-> (virtual-dpid,group-id)
 - Map of meter-id's to virtual meter-id's, meter-id <-> (virtual-dpid,meter-id)
 - Map of xid's so responses can be properly forwarded, xid <-> (virtual-switch,xid)
 - For all other physical switches, on what port to route packets
 - All virtual switches depending on this physical switch

## Async request filter
The asynchronous request filter saves per controller what unsollicited messages they want to receive.

# Events to handle
This section discusses some event that may happen that aren't directly openflow messages.

## New switch connection
 - Send port-up messages for all virtual ports on this switch
 - Rerun routing algorithm, update all routes

## Lost switch connection
 - Send port-down messages for all virtual ports on this switch
 - Rerun routing algorithm, update all routes

## New controller connection

## Lost controller connection

## New link found via topology discovery

## Link lost detected via topology discovery
 - Send port-down messages for all virtual ports using either link port
 - Rerun routing algorithm, update all routes

# Notes
Let all xid live in the system for a while so you know to what tenant to forward an error when you get 1

Some modification messages generate multiple modification messages to the switches.
If an error exists in those messages how to make sure the tenant only receives 1 error message?
What to do with messages that get accepted by only a subset of all the switches?

Need a way to easily lookup physical switches by datapath id

TODO Remove this section
