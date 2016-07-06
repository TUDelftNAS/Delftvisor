This document should summarize the design decisions taken when building the Hypervisor.

# Abstraction

## Flowtable abstraction

## Addres space abstraction
Always rewrite to internal representation at edge
If the in or out port is connected to a switch

## 

## Topology abstraction

# Flowtable layout
The following section describes the layout of flow rules the hypervisor.

## Openflow 1.3

Table 0:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
30 | Forward packet between switches | variable |
20 | Rewrite addresses at network edge, forward to personal flowtables | variable, timeout | eth-src=x, eth-dst=y | meter(n), eth-src=a, eth-dst=b, goto-tbl(n*k+1)
10 | 
 0 | New device | 1 | everything | output(controller)

Table n*k+1 to (n+1)*k:

Tables of tenant n tables

Meter tables:

The first n meter tables are reserved where n is the number of slices.
All these have 1 meter band with action drop when the rate comes above the allowed rate for the slice.

## Openflow 1.0
The Openflow 1.0 switches

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
0 | New device | 1 | everything | output(controller)

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

Only forward if the controller Async request filter says the controller wants to receive these packets.

### FlowRemoved

Only forward if the controller Async request filter says the controller wants to receive these packets.

### PortStatus

Only forward if the controller Async request filter says the controller wants to receive these packets.

# Data necessary
This section lists what data needs to be saved in the Hypervisor to function.

## Async request filter
The asynchronous request filter saves per controller what unsollicited messages they want to receive.

