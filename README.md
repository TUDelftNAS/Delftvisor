# Hypervisor

This project is an openflow hypervisor that works with openflow 1.3 leveraging the new features of openflow 1.3. It is a proof-of-concept implementation.

## Project structure
 - /src Contains all source code
 - /design Contains all design documents

## Dependencies
This project depends on several other programs for building and running:

 - cmake
 - gcc
 - boost (asio, system, program\_options, property\_tree)

## Building
This project uses cmake. Running the following commands ran from this directory produces an executable:

```
mkdir build/
cd build/
cmake ..
make
```

## Running
The Hypervisor uses a configuration file to setup all virtual networks. This configuration file is passed as a command line argument.

## Missing features
The current implementation is missing a bunch of features making it not officially Openflow 1.3 compatible and not usable in a production environment. The following is in incomplete list of missing features:

 - No statistics support, the multipart message is ignored
 - No TLS support
 - Roles and multiple connections are not properly supported
 - Cannot change configuration in running Hypervisor

## License
This program is licensed under GPL V3, which can be found as the file COPYING in this directory.
