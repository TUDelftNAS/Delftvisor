# Abstraction

## Flowtable abstraction

## Addres space abstraction
Always rewrite to internal representation at edge
If the in or out port is connected to a switch

## 

## Topology abstraction

## Topology abstraction

# Flowtable layout
The following section describes the layout of flow rules the hypervisor.

## Openflow 1.3

Table 1:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
20 | Forward packet between switches | variable |
10 | Rewrite addresses at network edge | variable, timeout |
 0 | New device | 1 | everything | send-to-controller



## Openflow 1.0
The Openflow 1.0 switches

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
0 | New device | 1 | everything | send-to-controller
