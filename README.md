# README

BBR' source code and test scripts for ns-3 (version 3.27).

See the [WNS3 Workshop paper (CCL18b)](#ccl18b) for a peer-reviewed
overview.

See the [Technical Report (CCL18a)](#ccl18a) for details.

-- Mark Claypool [claypool@cs.wpi.edu](claypool@cs.wpi.edu)

-----------------------------------
[Directories](#dir) | [Install](#inst) | [Misc](#misc) | [ToDo](#todo) | [Referenes](#refs)
<hr>


## <a name="dir"></a>DIRECTORIES

tests/ - test scripts for BBR'.

src/ - BBR' source code and ns-3 source files that need changes.

patch/ - directory with patchfile for ns-3 allinone tree.


## <a name="inst"></a>INSTALL

These instructions are to install BBR' to the ns-allinone package
(version 3.27).  Mileage may vary for alternate uses.

**ROOT** = ns-3 all-in-one directory

    e.g., /home/claypool/ns-allinone-3.27

**REPO** = this git repository

To install, either see [Applying Repository Patch](#apply) or [Linking
to Repository](#link).

### <a name="apply">Applying Repository Patch</a>

0) Download the latest "all in one" zip file from:

    https://www.nsnam.org/releases/

1) Extract:

    tar xvjf *.tar.bz2

2) Apply patch:

    patch -p0 < REPO/patch/*.patch

2) Go to ROOT and build:

    cd ROOT/

    ./build.py --enable-examples --enable-tests

3) Run and analyze.  Example:

    cd ROOT/ns-3.27/

    ./waf --run bulk 

### <a name="apply"></a>Linking to Repository

0) Download the latest "all in one" zip file from:

    https://www.nsnam.org/releases/

1) Extract:

    tar xvjf *.tar.bz2

2) Go to ROOT and build:

    cd ROOT/

    ./build.py --enable-examples --enable-tests

3) Go to ROOT source(s) and link in BBR' and supporting files:

    cd ROOT/ns-3.27/src/internet/model

    ln -s REPO/src/bbr/*.cc .

    ln -s REPO/src/bbr/*.h .

    ln -sf REPO/src/internet/*.cc .

    ln -sf REPO/src/internet/*.h .

    ln -sf REPO/src/internet/*.h .

(If using APP_PACING config, do below):

    cd ROOT/ns-3.27/src/applications/model

    ln -sf REPO/src/applications/*.cc .

    ln -sf REPO/src/applications/*.h .

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


## <a name="misc"></a>MISC

### Configurations

#### Pacing

There are three possible pacing configurations for BBR':

1) TCP_PACING. Packet pacing is done in TCP, as per the BBR' Technical
Report [CCL18a](#ccl18a).  This is the default configuration.

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

    ROOT/ns-3.27/src/internet/model/tcp-socket-base.h

e.g., 

    const enum_pacing_config PACING_CONFIG = NO_PACING;

#### Timing

There are two possible configurations for the round-trip time
used in culling the bandwidth window.

1) PACKET_TIME. Bandwidth window culling is done with a count of
"packet-timed" round-trips, as described in [CCYJ17](#ccyj17).  This
is the default configuration.

2) WALLCLOCK_TIME.  Bandwidth window culling is done using the
wall-clock for timing.  In this configuration, bandwidth estimates
that are older than the minimum round-trip time are removed from the
bandwidth window.

Time configurations for bandwidth culling are controlled via 
TIMING_CONFIG, defined in tcp-bbr.h. e.g.,
  
    const enum_time_config TIME_CONFIG = PACKET_TIME;


### Buffer Limits

Note, the fixed limits TCP receive and send buffers can restrict
throughput for some ns-3 simulations (and real TCP connections).  In
order to have the congestion window (i.e., the bottleneck bandwidth)
be the limit instead, the send/receive buffers can be increased from
their defaults (128k in ns-3).

To do so, change the ns-3 attributes "SndBufSize" and "RcvBufSize" via
script parameters.  For example, to double the default size (131072
bytes):

    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(131072*2));

    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(131072*2));

For reference, the source file that sets these values is:

    ROOT/ns-3.27/src/internet/model/tcp-socket.cc


## <a name="todo"></a>TODO

Below are BBR features *not* currently supported by BBR' (section
numbers below refer to [CCYJ17](#ccyj17)):

+ BBR' assumes that the application always has data to send, thus the
transmission rate is limited by the congestion control algorithm, not
the application.  BBR includes features to handle transmissions that
are application-limited.  See Section 4.1.1.4.  This also means BBR'
does not support restarting a flow from idle, a special case in BBR.
See Section 4.3.4.4.

+ BBR' only transitions to/from PROBE_RTT from PROBE_BW, while BBR has
additional transitions for PROBE_RTT.  See Section 4.3.5.

+ BBR' does not include the "send quantum" in BBR which is used to
amortize per-packet host overheads involved in the sending process.
The send quantum parameter can be helpful at low rates with small
packets.  See Section 4.2.2.

## <a name="refs"></a>REFERENCES

[CCG+17] N. Cardwell, Y. Cheng, C.S. Gunn, S.H. Yeganeh, and
V. Jacobson.  [BBR: Congestion-Based Congestion
Control](https://goo.gl/PLN3fA), *Communications of the ACM*, 60(2),
February 2017.

[<a name="ccl18a">CCL18a</a>] M. Claypool, J.W. Chung, and F. Li.
"BBR' - An Implementation of Bottleneck Bandwidth and Round-trip Time
Congestion Control for ns-3", Technical Report WPI-CS-TR-18-01,
Computer Science, Worcester Polytechnic Institute, January 2018.
Online: ftp://ftp.cs.wpi.edu/pub/techreports/pdf/18-01.pdf

[<a name="ccl18b">CCL18b</a>] M. Claypool, J.W. Chung, and F. Li.
[BBR' - An Implementation of Bottleneck Bandwidth and Round-trip Time
Congestion Control for
ns-3](http://www.cs.wpi.edu/~claypool/papers/bbr-prime/), In
Proceedings of the Workshop on ns-3 (WNS3), Mangalore, India,
June 2018. 

"BBR' - An Implementation of Bottleneck Bandwidth and Round-trip Time
Congestion Control for ns-3", Technical Report WPI-CS-TR-18-01,
Computer Science, Worcester Polytechnic Institute, January 2018.
Online: ftp://ftp.cs.wpi.edu/pub/techreports/pdf/18-01.pdf

[<a name="ccyj17">CCYJ17</a>] N. Cardwell, Y. Cheng, S. H. Yeganeh,
and V. Jacobson.  [BBR Congestion Control](https://goo.gl/dxV24D),
IETF Draft draft-cardwell-iccrg-bbr-congestion-control-00, July 2017.

-----------------------------------
[Directories](#dir) | [Install](#inst) | [Misc](#misc) | [ToDo](#todo) | [Referenes](#refs)
<hr>

