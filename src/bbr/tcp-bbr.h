/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 WPI, Verizon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef TCP_BBR_H
#define TCP_BBR_H

#include "tcp-congestion-ops.h"       
#include "tcp-bbr-state.h"            

namespace ns3 {

namespace bbr {

///////////////////////////////////////////////////////////////////

// Time configuration options (see Section 4.1.1.3 in [CCYJ17]):
// PACKET_TIME - Use packet-time RTT for culling BW window.
// WALLCLOCK_TIME - Use wall-clock RTT for culling BW window.
enum enum_time_config {WALLCLOCK_TIME, PACKET_TIME};

// Actual configuration option.
const enum_time_config TIME_CONFIG = PACKET_TIME;
//const enum_time_config TIME_CONFIG = WALLCLOCK_TIME;

///////////////////////////////////////////////////////////////////

// Constants.
const float VERSION = 1.5;            // See changelog.txt.
const Time INIT_RTT = Time(1000000);  // Nanoseconds (.001 sec).
const double INIT_BW = 6.0;           // Mb/s. 
const int RTT_WINDOW_TIME = 10;       // In seconds.
const int BW_WINDOW_TIME = 10;        // In RTTs.
const int MIN_CWND = 4;               // In packets.
const float PACING_FACTOR = 0.95;     // Factor of BW to pace (for tuning).
  
// PROBE_BW state:
// Gain rates per cycle: [1.25, 0.75, 1, 1, 1, 1, 1, 1]
const float STEADY_FACTOR = 1.0;      // Steady rate adjustment.
const float PROBE_FACTOR = 0.25;      // Add when probe.
const float DRAIN_FACTOR = 0.25;      // Decrease when drain.
  
// STARTUP state:
const float STARTUP_THRESHOLD = 1.25; // Threshold to exit STARTUP.
const float STARTUP_GAIN = 2.89;      // Roughly 2/ln(2).

// Structure for tracking TCP window for estimating BW.
struct packet_struct {
  SequenceNumber32 acked;  // Last sequence number acked.
  SequenceNumber32 sent;   // Next sequence number sent.
  Time time;               // Time sent.
  int delivered;           // Delivered bytes.
};

// Structure for storing BW estimates.
struct bw_struct {
  Time time;               // Time stored.
  int round;               // Virtual time stored.
  double bw_est;           // Bandwidth estimate.
};

} // end of namespace bbr
  
  
/**
 * \ingroup congestionOps
 *
 * \brief Implementation of basic TCP BBR' functionality.
 *
 */
class TcpBbr : public TcpCongestionOps {

public:

  friend class BbrState;
  friend class BbrStartupState;
  friend class BbrDrainState;
  friend class BbrProbeBWState;
  friend class BbrProbeRTTState;
  friend class BbrStateMachine;

  // Get type id.
  static TypeId GetTypeId(void);

  // Get name of congestion control algorithm.
  std::string GetName() const;

  // Default constructor.
  TcpBbr();

  // Copy constructor.
  TcpBbr(const TcpBbr &sock);

  // Destructor.
  virtual ~TcpBbr();

  // Before sending packet:
  // - Update TCP window
  // - Record information to estimate BW
  virtual void Send(Ptr<TcpSocketBase> tsb, Ptr<TcpSocketState> tcb);

  // On receiving ack, store RTT and estimated BW.
  // Compute and set pacing rate.
  virtual void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t packets_acked,
                         const Time &rtt);

  // Copy BBR' congestion control with copy.
  virtual Ptr<TcpCongestionOps> Fork();

  // BBR' ignores calls to increase window.
  virtual void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segs_acked);

  // BBR' does not use ssthresh, so ignored.
  virtual uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t b_in_flight);

private:
  
  // Check if should enter PROBE_RTT state.
  bool checkProbeRTT();

  // Return bandwidth-delay product (in Mbits).
  double getBDP() const;

  // Return round-trip time (min of window, in seconds).
  // Return -1 if no RTT estimates yet.
  Time getRTT() const;

  // Return bandwidth (max of window, in Mb/s).
  // Return -1 if no BW estimates yet.
  double getBW() const;

  // Remove BW estimates that are too old (greater than 10 RTTs).
  void cullBWwindow();

  // Remove RTT estimates that are too old (greater than 10 seconds).
  void cullRTTwindow();

protected:
  double m_pacing_gain;                    // Scale estimated BDP for pacing.
  double m_cwnd_gain;                      // Scale estimated BDP for cwnd.
  int m_round;                             // For recording virtual RTT time.
  int m_delivered;                         // For computing virtual RTT rounds.
  int m_next_round_delivered;              // For computing virtual RTT rounds.
  std::map<Time, Time> m_rtt_window;       // For computing min RTT.
  std::vector<bbr::bw_struct> m_bw_window; // For computing max BW.
  std::vector<bbr::packet_struct> m_pkt_window; // For estimating BW from ACKs.
  uint32_t m_bytes_in_flight;              // Bytes in flight (from socket base).
  Time m_min_rtt_change;                   // Last time min RTT changed.
  BbrStateMachine m_machine;               // State machine.
  BbrStartupState m_state_startup;         // STARTUP state.
  BbrDrainState m_state_drain;             // DRAIN state.
  BbrProbeBWState m_state_probe_bw;        // PROBE_BW state.
  BbrProbeRTTState m_state_probe_rtt;      // PROBE_RTT state.
};

} // end of namespace ns3

#endif // TCP_BBR_H
