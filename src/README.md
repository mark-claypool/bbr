README
======

BBR' source files.

## Directories

Source directory for BBR' and ns-3 files that need changing.

+ bbr/ - main BBR' directory.
+ internet/ - source code for tcp-sock-base+.
+ applications/ - source code for bulk transfer application.

## Configurations

There are three possible configurations for BBR':

1) TCP_PACING. Packet pacing is done in TCP, as per the BBR Tech
Report.  This is the default configuration.

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

    tcp-socket-base.h

e.g., 

    const enum_pacing_config PACING_CONFIG = NO_PACING;
