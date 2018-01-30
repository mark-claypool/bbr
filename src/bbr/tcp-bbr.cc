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

// System includes.
#include <iostream>

// NS includes.
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "tcp-socket-base.h"          // For pacing configuration options.

// BBR' includes.
#include "rtt-estimator.h"
#include "tcp-bbr.h"

using namespace ns3;

// Logging components. 
NS_LOG_COMPONENT_DEFINE("TcpBbr");
NS_OBJECT_ENSURE_REGISTERED(TcpBbr);

// Default constructor.
TcpBbr::TcpBbr(void) :
  TcpCongestionOps(),
  m_pacing_gain(0.0),
  m_cwnd_gain(0.0),
  m_bytes_in_flight(0),
  m_min_rtt_change(Time(0)),
  m_machine(this),
  m_state_startup(this),
  m_state_drain(this),
  m_state_probe_bw(this),
  m_state_probe_rtt(this) {

  NS_LOG_FUNCTION(this);
  NS_LOG_INFO(this << "  BBR' version: v" << bbr::VERSION);

  // Constants in "tcp-bbr.h"
  NS_LOG_INFO(this << "  INIT_RTT: " << bbr::INIT_RTT.GetSeconds() << " sec");
  NS_LOG_INFO(this << "  INIT_BW: " << bbr::INIT_BW << " Mb/s");
  NS_LOG_INFO(this << "  BW_WINDOW_TIME: " << bbr::BW_WINDOW_TIME << " rtts");
  NS_LOG_INFO(this << "  MIN_CWND: " << bbr::MIN_CWND << " packets");
  NS_LOG_INFO(this << "  STARTUP_THRESHOLD: " << bbr::STARTUP_THRESHOLD);
  NS_LOG_INFO(this << "  STARTUP_GAIN: " << bbr::STARTUP_GAIN);
  NS_LOG_INFO(this << "  STEADY_FACTOR: " << bbr::STEADY_FACTOR);
  NS_LOG_INFO(this << "  PROBE_FACTOR: " << bbr::PROBE_FACTOR);
  NS_LOG_INFO(this << "  DRAIN_FACTOR: " << bbr::DRAIN_FACTOR);
  NS_LOG_INFO(this << "  PACING_FACTOR: " << bbr::PACING_FACTOR);

  // Constant in "tcp-socket-base.h"
  if (PACING_CONFIG == NO_PACING) 
    NS_LOG_INFO(this << "  Note: BBR' configured with pacing NO_PACING.");

  // First state is STARTUP.
  m_machine.changeState(&m_state_startup);
}

// Copy constructor.
TcpBbr::TcpBbr(const TcpBbr &sock) :
  TcpCongestionOps(sock),
  m_pacing_gain(0.0),
  m_cwnd_gain(0.0),
  m_bytes_in_flight(0),
  m_min_rtt_change(Time(0)),
  m_machine(this),
  m_state_startup(this),
  m_state_drain(this),
  m_state_probe_bw(this),
  m_state_probe_rtt(this) {  
  NS_LOG_FUNCTION("[copy constructor]" << this << &sock);
}

// Default destructor.
TcpBbr::~TcpBbr(void) {
  NS_LOG_FUNCTION(this);
}

// Get type id.
TypeId TcpBbr::GetTypeId(void) {
  static TypeId tid = TypeId("ns3::TcpBbr")
    .SetParent<TcpCongestionOps>()
    .SetGroupName("Internet")
    .AddConstructor<TcpBbr>();
  return tid;
}

// Get name of congestion control algorithm.
std::string TcpBbr::GetName() const {
  NS_LOG_FUNCTION(this);
  return "TcpBbr";
}

// Copy BBR' congestion control algorithm across socket.
Ptr<TcpCongestionOps> TcpBbr::Fork() {
  NS_LOG_FUNCTION(this);
  return CopyObject<TcpBbr> (this);
}

// BBR' ignores calls to increase window.
// tcb = internal congestion state
void TcpBbr::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segs_acked) {
  NS_LOG_FUNCTION(this << tcb << segs_acked);
  NS_LOG_INFO(this << "  Ignored.");
  return;
}

// BBR' does not use ssthresh, so ignored.
// tcb = transmission control block
uint32_t TcpBbr::GetSsThresh(Ptr<const TcpSocketState> tcb,
                             uint32_t b_in_flight) {
  NS_LOG_FUNCTION(this << tcb << b_in_flight);
  NS_LOG_INFO(this << "  Ignored.  Returning max (65535).");
  return 65535;
}

// On receiving ack, store RTT and estimated BW.
// Compute and set pacing rate.
// tcb = transmission control block
void TcpBbr::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t packets_acked,
                       const Time &rtt) {

  NS_LOG_FUNCTION(this << packets_acked << rtt);

  ////////////////////////////////////////////
  // RTT
  if (rtt.IsZero() || rtt.IsNegative()) {
    NS_LOG_WARN("RTT measured is zero (or less)! Not stored.");
    return;
  }

  // See if changed minimum RTT (to decide when to PROBE_RTT).
  Time now = Simulator::Now();
  Time min_rtt = getRTT();
  if (rtt < getRTT()) {
    NS_LOG_INFO(this << "  New min RTT: " << 
                rtt << " sec (was: " << min_rtt.GetSeconds() << ")");
    m_min_rtt_change = now;  
  }

  // Add to RTT window.
  m_rtt_window[now] = rtt;

  // Upon first RTT, call update() to initialize timer.
  if (m_rtt_window.size() == 1) {
    NS_LOG_INFO(this << "  First rtt, calling update() to init.");
    m_machine.update();
  }

  ////////////////////////////////////////////
  // BW ESTIMATION
  // Based on [CCYJ17b]:
  // Cheng et al., "Delivery Rate Estimation", IETF Draft, Jul 3, 2017
  //
  // Send() for W_s:                       (outstanding)
  //   Have TCP Window, (latest acked) W_a [W_1 ... W_n] W_s (next sent)
  //   Record W_a
  //   Record time W_t
  //   Send W_s
  // PktsAcked() for W_s:
  //   Record time W_t'
  //   Compute BW: bw = (W_s - W_a) / (W_t' - W_t)
  //   Update data structures

  SequenceNumber32 ack = tcb->m_lastAckedSeq;  // W_s
  now = Simulator::Now();                      // W_t'

  // Find newest ack in window, <= current. Note, window is sorted.
  bbr::bw_est_struct temp;
  for (auto it = m_est_window.begin(); it != m_est_window.end(); it++)
    if (it->sent <= ack)                       // W_a
      temp = *it;

  // Remove all acks <= current from window.
  for (unsigned int i=0; i < m_est_window.size(); )
    if (m_est_window[i].sent <= temp.sent) 
      m_est_window.erase(m_est_window.begin() + i);
    else
      i++;
        
  // Estimate BW: bw = (W_s - W_a) / (W_t' - W_t)
  double bw_est = (ack - temp.acked) /
                  (now.GetSeconds() - temp.time.GetSeconds());
  bw_est *= 8;          // Convert to b/s.
  bw_est /= 1000000;    // Convert to Mb/s.

  // Add to BW window.
  m_bw_window[now] = bw_est;

  // Set pacing rate (in Mb/s), adjusted by gain.
  double pacing_rate = getBW() * m_pacing_gain;

  // There may be some advantages to pacing at less than the BW.
  // Either way, this is adjustable in the header file.
  if (m_pacing_gain == 1)
    pacing_rate *= bbr::PACING_FACTOR;
  
  if (pacing_rate < 0)
    pacing_rate = 0.0;
  if (PACING_CONFIG != NO_PACING)
    tcb -> SetPacingRate(pacing_rate);

  // Report data.
  NS_LOG_INFO(this << "  W_s: " << ack);
  NS_LOG_INFO(this << "  W_a: " << temp.acked);
  NS_LOG_INFO(this << "  Time (W_t): " << now.GetSeconds() << " seconds");
  NS_LOG_INFO(this << "  W_s time (W_t'): " << temp.time.GetSeconds() << " seconds");
  NS_LOG_INFO(this << "  byte-diff: " << (ack - temp.acked));
  NS_LOG_INFO(this << "  time-diff: " << (now.GetSeconds() - temp.time.GetSeconds()));
  
  NS_LOG_INFO(this << "  bw: " << bw_est);
  
  NS_LOG_INFO(this << "  DATA rtt: " << rtt.GetSeconds() << "  " <<
              "pacing-gain " << m_pacing_gain <<  "  " <<
              "pacing-rate " << pacing_rate << " Mb/s  " <<
              "bw: " << bw_est << " Mb/s");
}

// Before sending packet:
// - Update TCP window
// - Record information to estimate BW
// tsb = tcp socket base
// tcb = transmission control block
void TcpBbr::Send(Ptr<TcpSocketBase> tsb, Ptr<TcpSocketState> tcb) {

  NS_LOG_FUNCTION(this);

  ////////////////////////////////////////////
  // BW ESTIMATION
  //
  // Record information to estimate BW upon ACK (in PktsAcked()).
  //
  // Based on [CCYJ17b]:
  // Based on:
  // Cheng et al., "Delivery Rate Estimation", IETF Draft, Jul 3, 2017
  //
  // Send() for W_s:                       (outstanding)
  //   Have TCP Window, (latest acked) W_a [W_1 ... W_n] W_s (next sent)
  //   Record W_a
  //   Record time W_t
  //   Send W_s
  // PktsAcked() for W_s:
  //   Record time W_t'
  //   Compute BW: bw = (W_s - W_a) / (W_t' - W_t)
  //   Update data structures

  // Get the bytes in flight (needed for STARTUP).
  m_bytes_in_flight = tsb -> BytesInFlight();

  // Get last sequence number acked.
  bbr::bw_est_struct bw_est;
  bw_est.acked = tcb -> m_lastAckedSeq;
  bw_est.sent = tcb -> m_nextTxSequence;
  bw_est.time = Simulator::Now();
  m_est_window.push_back(bw_est);
  
  NS_LOG_INFO(this << "  Last acked seq: " << bw_est.acked);
  NS_LOG_INFO(this << "     Sending seq: " << bw_est.sent);

  double cwnd;
  double bdp = 0.0;

  ////////////////////////////////////////////
  // SET TCP CONGESTION WINDOW (CWND).
  
  // Special case: if in PROBE_RTT state, set window to minimum.
  if (m_machine.getStateType() == bbr::PROBE_RTT_STATE) {

    cwnd = bbr::MIN_CWND * 1500; // In bytes.
    NS_LOG_INFO(this << "  In PROBE_RTT. Window min: " << bbr::MIN_CWND << " pkts");

  } else {

    // Compute TCP cwnd based on BDP and gain.
    bdp = getBDP();
    if (PACING_CONFIG == NO_PACING)
      // If no pacing, cwnd is used to control pace.
      cwnd = bdp * m_pacing_gain;
    else
      // If pacing, cwnd adjusted larger.
      cwnd = bdp * m_cwnd_gain;
    cwnd = cwnd * 1000000 / 8; // Mbits to bytes.

    // Make sure cwnd not too small (roughly, 4 packets).
    if ((cwnd / 1500) < bbr::MIN_CWND) {
      NS_LOG_INFO(this << "  Boosting cwnd to 4 x 1500B packets.");
      cwnd = bbr::MIN_CWND * 1500; // In bytes.
    }

  }

  // Set cwnd (in bytes).
  tcb -> m_cWnd = (uint32_t) cwnd;

  NS_LOG_INFO(this << "  DATA " <<
              "bdp: " << bdp << " (Mb), " << bdp * 1000000/8 << " (B)  " <<
              "cwnd-gain " << m_cwnd_gain <<  "  " <<
              "cwnd " << cwnd << " (B)  "
              "bytes-in-flight " <<  m_bytes_in_flight);
}

// Return bandwidth (maximum of window, in Mb/s).
// Return -1 if no BW estimates.
double TcpBbr::getBW() const {
  double max_bw = 0;

  NS_LOG_FUNCTION(this);

  if (m_bw_window.size() == 0)

    // Special case if no BW estimates.
    max_bw = -1.0;

  else

    // Find max BW in window.
    for (auto it = m_bw_window.begin(); it != m_bw_window.end(); it++)
      max_bw = std::max(max_bw, it->second);
  
  NS_LOG_INFO(this << "  DATA bws in window: " << m_bw_window.size() <<
              "  max_bw: " << max_bw);

  // Return it.
  return max_bw;
}

// Return round-trip time (min of window, in seconds).
// Return -1 if no RTT estimates.
Time TcpBbr::getRTT() const {
  Time min_rtt = Time::Max();

  NS_LOG_FUNCTION(this);

  if (m_rtt_window.size() == 0)

    // Special case if no RTT estimates.
    min_rtt = Time(-1.0);

  else
    
    // Find minimum RTT in window.
    for (auto it = m_rtt_window.begin(); it != m_rtt_window.end(); it++)
      min_rtt = std::min(min_rtt, it->second);

  NS_LOG_INFO(this << "  DATA rtts in window: " << m_rtt_window.size() <<
              "  min_rtt: " << min_rtt.GetSeconds());

  // Return it.
  return min_rtt;
}

// Return bandwidth-delay product (in Mbits).
double TcpBbr::getBDP() const {
  NS_LOG_FUNCTION(this);
  Time rtt = getRTT();
  if (rtt.IsNegative())
    rtt = bbr::INIT_RTT;
  double bw = getBW();
  if (bw < 0)
    bw = bbr::INIT_BW;
  return (double) (rtt.GetSeconds() * bw);
}

// Remove BW estimates that are too old (greater than 10 RTTs).
void TcpBbr::cullBWwindow() {

  NS_LOG_FUNCTION(this);

  // If no BW estimates, leave window unchanged.
  double bw = getBW();
  if (bw < 0)
    return;

  // If no RTT estimates, leave window unchanged.
  Time rtt = getRTT();
  if (rtt.IsNegative())
    return;

  // Compute time delta, 10 RTTs ago until now.
  Time now = Simulator::Now();
  Time delta = now - rtt * bbr::BW_WINDOW_TIME;

  // Erase any values that are too old.
  auto it = m_bw_window.begin();
  while (it != m_bw_window.end()) {
    if (it -> first < delta)
      it = m_bw_window.erase(it);
    else
      it++;
  }
 
  int size = m_bw_window.size();
  if (size == 0)
    NS_LOG_INFO(this << " BW window empty.");
  else
    NS_LOG_INFO(this << " DATA" <<
                "  m_bw_window_size: " << size <<
                " [" << m_bw_window.begin()->first.GetSeconds() << ", " <<
                m_bw_window.rbegin()->first.GetSeconds() << "]");
}

// Remove RTT estimates that are too old (greater than 10 seconds).
void TcpBbr::cullRTTwindow() {

  NS_LOG_FUNCTION(this);

  // If no RTT estimates, leave window unchanged.
  Time rtt = getRTT();
  if (rtt.IsNegative())
    return;

  // Compute time delta, 10 seconds ago until now.
  Time now = Simulator::Now();
  Time delta = Time(now - bbr::RTT_WINDOW_TIME * 1000000000.0); // Units are nanoseconds.

  // Erase any values that are too old.
  auto it = m_rtt_window.begin();
  while (it != m_rtt_window.end()) {
    if (it -> first < delta)
      it = m_rtt_window.erase(it);
    else
      it++;
  }
 
  int size = m_rtt_window.size();
  if (size == 0)
    NS_LOG_INFO(this << " RTT window empty.");
  else
    NS_LOG_INFO(this << " DATA" <<
                "  m_rtt_window_size: " << size <<
                " [" << m_rtt_window.begin()->first.GetSeconds() << ", " <<
                m_rtt_window.rbegin()->first.GetSeconds() << "]");
}

// Return true if should enter PROBE_RTT state.
bool TcpBbr::checkProbeRTT() {

  NS_LOG_FUNCTION(this);

  // In PROBE_BW state and min RTT hasn't changed in 10 seconds.
  Time now = Simulator::Now();
  if (m_machine.getStateType() == bbr::PROBE_BW_STATE &&
      (now.GetSeconds() - m_min_rtt_change.GetSeconds()) > 10) {

    NS_LOG_INFO(this << "  min RTT last changed: " << m_min_rtt_change.GetSeconds());

    m_min_rtt_change = now;

    return true;
  }

  return false;
}
  

