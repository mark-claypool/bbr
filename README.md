# README

BBR' source code and test scripts for ns-3 (version 3.27).

-- Mark Claypool [claypool@cs.wpi.edu](claypool@cs.wpi.edu)


## DIRECTORIES

tests/ - test scripts for BBR'.
src/ - BBR' source code and ns-3 source files that need changes.
patch/ - directory with patchfile for ns-3 allinone tree.


## TO INSTALL

These instructions are to install BBR' to the ns-allinone package
(version 3.27).  Mileage may vary for alternate uses.

NS3 = ns-3 all-in-one directory
    e.g., /home/claypool/ns-allinone-3.27
REPO = this git repository

### Applying Repository Patch

1) Extract ns allinone:

    bzip2 -d *.zip
    tar xvf *tar

2) Apply patch:

    patch -p0 < REPO/*.patch

3) Run and analyze.  Example:

    cd ROOT/ns-3.27/
    ./waf --run bulk 

### Linking to Repository

1) Extract ns allinone:

    bzip2 -d *.zip
    tar xvf *tar
	
2) Go to NS3 and build:

    cd NS3/
    ./build.py --enable-examples --enable-tests

3) Go to NS3 source and link in BBR' and supporting files:

    cd NS3/ns-3.27/src/internet/model
	ln -s REPO/src/bbr/*.cc .
	ln -s REPO/src/bbr/*.h .
	ln -sf REPO/src/model/*.cc .
	ln -sf REPO/src/model/*.h .
	ln -sf REPO/src/model/*.h .

    cd NS3/ns-3.27/src/applications/model
	ln -sf REPO/src/applications/*.cc .
	
4) Edit script file to build BBR':

    edit ROOT/ns-3.27/src/internet/wscript

  Add (line 152):

        'model/tcp-bbr.cc',
        'model/tcp-bbr-state.cc',

  Add (line 380):

          'model/tcp-bbr.h',
          'model/tcp-bbr-state.h',

5) Link BBR' script test:

    cd ROOT/ns-3.27/examples/
    ln -s REPOSITORY/bbr-tests .

6) Run and analyze.  Example:

    cd ROOT/ns-3.27/
    ./waf --run bulk 


## MISC

### Configurations

There are three possible configurations for BBR':

1) TCP_PACING. Packet pacing is done in TCP, as per the BBR' Technical
Report [CCL18].  This is the default configuration.

2) APP_PACING. Packet pacing is NOT done in TCP.  BBR changes the
pacing rate and still expects packets to be paced, but in this
configuration, the application layer must do the pacing.  The
application directory provides a BulkSendApplication that does pacing.

3) NO_PACING. Pacing is not done at all.  In this configuration, the
BBR' code changes slightly.  Instead of having a larger cwnd and
having the pacing rate control the bandwidth, the cwnd itself is used
to control the bandwidth.  Thus, the cwnd is set to the BDP (with any
needed gain adjustments) as the sole way of controlling the rate.

Configurations are controlled via PACING_CONFIG in:

    NS3/ns-3.27/src/internet/model/tcp-socket-base.h

e.g., 

    const enum_pacing_config PACING_CONFIG = NO_PACING;

### Buffer Limits

Note, the TCP receive and send buffers can often limit the throughput
for ns-3 simulations (and real TCP connections).  In order to have the
congestion window (i.e., the bottleneck bandwidth) be the limit, the
send/receive buffers can be increased from their defaults (128k in
ns-3).

The file that holds these values.

    ROOT/ns-3.27/src/internet/model/tcp-socket.cc
	
Changing the attributes "SndBufSize" and "RcvBufSize". These values
can also be changed via script parameters.


## REFERENCES

[CCG+17] N. Cardwell, Y. Cheng, C.S. Gunn, S.H. Yeganeh, and
V. Jacobson.  "BBR: Congestion-Based Congestion Control",
*Communications of the ACM*, 60(2), February 2017.

[CCL18] M. Claypool, J. Chung, and F. Li. "BBR' - An Implementation of
Bottleneck Bandwidth and Round-trip Time Congestion Control for ns-3",
Technical Report WPI-CS-TR-18-01, Computer Science, Worcester
Polytechnic Institute, January 2018.

[CCYJ17] N. Cardwell, Y. Cheng, S. H. Yeganeh, and V. Jacobson.  "BBR
Congestion Control", *IETF Draft
draft-cardwell-iccrg-bbr-congestion-control-00*, July 2017.

