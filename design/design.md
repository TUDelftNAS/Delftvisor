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

This is achieved by metering a packet before it is forwarded to a slice's flow tables. This hard limits the amount of packets that each slice can insert. The maximum rate a slice can use is pre-configured and used throughout the entire network.

More fine-grained bandwidth isolation remains future work. A problem with the current approach is that even if the network has the capability to allow a slice to use more traffic is it not utilized. During configuration an error might be made reserving more traffic over a link than is actually possible. This is currently not detected and would allow slices to influence each others traffic.

## Address space isolation
Each slice needs to be able to use the full Ethernet/IP address space without clashing with another slice.

OpenVirtex, an Openflow 1.0 Hypervisor, achieves this by rewriting the source and destination addresses at the edge of the network to an internal representation and rewriting it again before the packet is output again at the network edge. With Openflow 1.3 this becomes difficult when rewriting packets that are output via a group table (and some other problems) which is why a different concept was chosen.

PBB (Provider Backbone Bridging, also called mac-in-mac) was chosen because it allows for 24 bits that support masking. Openflow switches are not required to allow masking on MPLS tags which would be necessary for efficient routing.

PBB allows for an extra pair of source/destination mac addresses in a packet and also adds a 24 bit I-SID of which all bits are maskable. This allows for 120 bits of extra information per packet. These would be split up using:
 - 1 bit goal (0=flowtable,1=output-port), use the entire I-SID field for this so it can be changed in one set-field action
 - 32 bit switch id
 - 32 bit port id
 - 32 bit slice id

### Vlan tagging
This section describes the original concept with VLAN tagging. In this concept 1 VLAN tag was added to each packet which contained all the information the Hypervisor needed to handle the packet. Unfortunately does Openflow 1.0 only allow VLAN tags and not MPLS tags or PBB tags, but even worse it only allows tagging a packet with 1 VLAN tag. This leads to the following distribution of bits:

Openflow allows matching against the 12 VLAN id bits and 3 VLAN priority bits, so 15 bits per tag.

VLAN tag 15 bits:
 - 4 bit Slice id (16 values)
 - 1 bit goal (0=flowtable,1=exit)
 - 5 bit Switch target (32 values)
 - 5 bit Port target (32 values)

Which is not enough slices, switches and ports. No formal requirement was set for the amount of slices, switches and ports but any data centre is already bigger than this which is why this concept was abandoned.

### Double VLAN tagging
This section describes a concept that uses multiple VLAN tags, this concept was infeasible because Openflow 1.0 only supports 1 VLAN tag.

First VLAN tag 15 bits:
 - 5 bit Slice id (32 values)
 - 1 bit what-vlan-tag (always 0, indicating this is the first VLAN tag)
 - 9 bit Switch target (512 values)

Second VLAN tag 15 bits:
 - 5 bit Slice id (32 values)
 - 1 bit what-vlan-tag (always 1, indicating this is the second VLAN tag)
 - 1 bit goal (0=flowtable,1=exit)
 - 8 bit Port target (256 values)

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
60 | Forward inter-virtual-switch traffic | # of ports with links \* # of switches | in-port=x, pbb-switch-bits=y | output(z)
50 | Output already processed packet over port with link | # of ports with link \* # of ports with link | in-port=x (port with link), pbb-goal-bit=0, pbb-switch-bits=a, pbb-port-bits=b | pop-pbb, output(y) (port without link)
40 | Output already processed packet over port without link | # of ports with link \* # of ports without link | in-port=x (port with link), pbb-goal-bit=1, pbb-switch-bits=a, pbb-port-bits=b | pbb-goal-bit=0, output(y) (port without link)
30 | Forward tagged packets to flowtable | # of ports with links \* # slices | pbb-slice-bits=x | meter(n), goto-tbl(n\*k+1)
20 | Detect erroneous packets on switch links, cookie=2 | # of ports with link | in-port=x | output(controller)
10 | Forward new packet to personal flowtables | # of ports without link | in-port=x | meter(n), goto-tbl(n\*k+1)
 0 | Error detection rule, cookie=3 | 1 | * | output(controller)

## Table n\*k+1 to (n+1)\*k-1, tenant tables:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
- | Rewritten flow rules from tenant controller | - | - | -

How to rewrite flow rules:
```
If apply-action instruction:
  If an output action and output over switch-switch link:
    Replace with push-pbb, set-field(eth-dst,eth-src,pbb-isid), output(new-port), pop-pbb
    This might be optimized by detection if the tags really needs to be popped
  If an output action and output over host port:
    Replace with output(new-port)
If write-action instruction:
  Rewrite output action to group action
If instruction list contains goto-tbl:
  Rewrite goto-tbl number
```

## Group tables

Purpose | Id | Amount | Mode | Buckets
--------+----+--------+------+--------
Simulate FLOOD output action | - | # of slices | All | Bucket(Output(x)), etc
Add outgoing VLAN tag | Copy tag bit structure | # of slices \* # of ports in network not on this switch \* # of ports | Indirect | Bucket(push-pbb, pbb-goal-bit=0, pbb-switch-bits=a, pbb-port-bits=b, output(c))

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

### BarrierRequest
See EchoRequest, do the same thing.

### Experimenter
Return an error with type BadRequest and code BadExperimenterType.

### FeatureRequest
Return an FeatureReply with:
 - datapath_id   = Virtual switch datapath id
 - n_buffers    = minimum n_buffers of underlying physical switches
 - n_tables     = Amount of tables reserved for this slice
 - auxiliary_id = 0
 - capabilities = FLOW_STATS | TABLE_STATS | GROUP_STATS | (IP_REASM if all underlying switches support it)

Cannot provide PORT_STATS since other slices can sent packets through the same port.

TODO Maybe can provide QUEUE_STATS?

### GetConfigRequest
TODO Don't know what to do with this, probably just not support it or expose the most restrictive policy supported by all physical switches below this one.

### SetConfig
TODO Probably sent an error since this would affect all packets going through the switch instead of 1 slice.

### PacketOut
Scan the action list, if there is an output() action rewrite port to physical port and figure out physical switch.
If there is a group action sent to closest physical openflow 1.3 switch.

If packet is sent to network edge port don't rewrite, otherwise add ethernet addresses to slice rewrite list and rewrite ethernet addresses in the packet.

TODO Are the actions filters enough?

Sent packet to physical switch.

### FlowMod

If table_id=OFPTT_ALL create clone messages for each table assigned to this slice.
If command=OFPC_DELETE* rewrite the out_port field and out_group field

The flags field can be directly forwarded.
TODO Should the cookie field be rewritten?

### GroupMod

### PortMod

TODO Don't allow. Ports may be used by multiple slices.

### TableMod
This message is deprecated in openflow 1.3, don't do anything.

### MeterMod
The meter identifier needs to be rewritten to a meter id allocated to the slice.
If the meter id was out of the allowed range of meters for the slice the hypervisor will sent back an Error with type MeterModFailed and code OutofMeters.
If the meter id is OFMP_ALL simulate it by deleting all meters in the slice range.
The packet can then be forwarded.

TODO simulating OFMP_ALL is slow, dealing with the response/failure messages of that seems difficult and the standard doesn't say you need to use adjacent meter id's.

### MultipartRequest
TODO Just don't support it, openvswitch doesn't support it either.

### QueueGetConfigRequest

### RoleRequest
Send an error message with type RoleRequestFailed and code Unsupported.

### GetAsyncRequest
Retreive the setting from the controller structure and send a GetAsyncResponse message with these settings in them.

### SetAsync
Save the new settings in the controller structure.

## Packets from a switch
This section discusses the actions to perform on packets that are received unsollicited from a switch.
The packets EchoResponse, FeatureResponse, GetConfigResponse, QueueGetConfigResponse are already discussed in the previous section.

### PacketIn
Determine if this is caused by an address that needs to be rewritten.
If table_id=0 & cookie=1 the packet comes from topology discovery.
  Mark the link as live, reset liveness timer, drop packet.
Else
  Log packet, show error

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

## Async request filter
The asynchronous request filter saves per controller what unsollicited messages they want to receive.

