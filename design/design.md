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
