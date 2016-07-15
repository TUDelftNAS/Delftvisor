# Hypervisor

This project is an openflow hypervisor that works with openflow 1.3 leveraging the new features of openflow 1.3. It is a proof-of-concept implementation.

## Project structure
 - /src Contains all source code
 - /design Contains all design documents

## Dependencies


## Building
This project uses cmake. Running the following commands ran from this directory produces an executable:

    mkdir build/
    cd build/
    cmake ..
    make

## Running
The Hypervisor uses a configuration file to setup all virtual networks. This configuration file is passed as a command line argument.

## License
This program is licensed under GPL V3, which can be found as the file COPYING in this directory.
