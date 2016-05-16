# Abstraction

## Flowtable abstraction

## 

## Topology abstraction

## Addres space abstraction
Always rewrite to internal representation at edge
If the in or out port is connected to a switch

## Topology abstraction

# Flowtable layout
This document describes the layout of flow rules the hypervisor.

## Openflow 1.0

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
0 | New device | 1 | everything | send-to-controller

## Openflow 1.3

Table 1:

Priority | Purpose | Amount | Match | Instructions
---------|---------|--------|-------|-------------
20 | Forward packet between switches | variable |
10 | Rewrite addresses at network edge | variable, timeout |
0 | New device | 1 | everything | send-to-controller

Other tables:
Directly put in rules from the top controllers
