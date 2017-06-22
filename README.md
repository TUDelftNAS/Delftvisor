# Delftvisor

Delftvisor is an openflow hypervisor that works with openflow 1.3 leveraging the new features of openflow 1.3. It is a proof-of-concept implementation produced for a master thesis, its design is discussed in the thesis available from the [TU Delft repository](https://repository.tudelft.nl/islandora/object/uuid:66db3d64-b3df-42d6-a468-c6cf50558e55).

This project is currently unmaintained.

## Project structure
 - /src Contains all source code
 - /configuration Contains some configuration examples
 - /design Contains some design documents, the thesis in the [TU Delft repository](https://repository.tudelft.nl/islandora/object/uuid:66db3d64-b3df-42d6-a468-c6cf50558e55) contains more details.

## Building and running
This project uses cmake. Running the following commands in Ubuntu 16.04 LTS produces a delftvisor executable:

```
sudo apt-get install gcc cmake git libboost-all-dev autoconf mininet
git clone https://github.com/TUDelftNAS/Delftvisor.git
mkdir Delftvisor/build/
cd Delftvisor/build/
cmake ..
make
```

The Delfvisor executable is now at Delftvisor/build/src/delftvisor.

### Running an experiment
If you have followed the instructions above you can run the following commands to perform the linear 4,2 experiment. Delftvisor is very much proof-of-concept software and has only been tested with controllers using the Ryu framework. Delftvisor has worked with a simple L2 router available at [https://github.com/harmjan/l2-router](https://github.com/harmjan/l2-router).

 - Run the command `sudo mn --controller remote,ip=127.0.0.1 --switch ovsk,protocols=OpenFlow13 --mac --topo linear,4,2` in a separate terminal
 - Run the command `./src/delftvisor -l info ../configuration/linear_4_2.json` from the build directory
 - Run the controllers on port 6653 and 6654 in separate terminals

The experiment is now running, you can type the `pingall` command in mininet to see which hosts can now see each other. You can bring down links in mininet to see how Delftvisor and the controllers react to it.

## Missing features
The current implementation is missing a bunch of features making it not officially Openflow 1.3 compatible and not usable in a production environment. The following is in incomplete list of missing features:

 - No statistics support, the multipart message is ignored
 - No TLS support
 - Roles and multiple connections are not properly supported
 - Cannot change configuration while running Delftvisor
 - No multi-threading
 - No input validation on network packets, sending malformed Openflow packets will crash Delftvisor
 - There are still known situations where Delftvisor crashes

## License
This program is licensed under GPL V3, which can be found as the file COPYING in this directory.
