//           
// Network topology
//
//       n0 ------------ (n1/router) -------------- n2
//            10.1.1.x                192.168.1.x
//       10.1.1.1    10.1.1.2   192.16.1.1     192.168.1.2
//
// - Flow from n0 to n2 using BulkSendApplication.
//
// - Tracing of queues and packet receptions to file "*.tr" and
//   "*.pcap" when tracing is turned on.
// 
// - Change of bandwidth UP partway through, then DOWN before end.
//

// System includes.
#include <string>
#include <fstream>

// NS3 includes.
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

// Constants.
#define VZ_ENABLE_PCAP      true      // Set to "false" to disable pcap
#define VZ_ENABLE_TRACE     true      // Set to "false" to disable trace
#define VZ_BIG_QUEUE        2000      // Packets
#define VZ_QUEUE_SIZE       100       // Packets
#define VZ_START_TIME       0.0       // Seconds
#define VZ_STOP_TIME        5.0       // Seconds
#define VZ_S_TO_R_BW        "150Mbps" // Server to router
#define VZ_S_TO_R_DELAY     "10ms"
#define VZ_R_TO_C_BW        "10Mbps"  // Router to client (bttlneck)
#define VZ_R_TO_C_DELAY     "1ms"
#define VZ_PACKET_SIZE      1000      // Bytes.

// Uncomment one of the below.
#define VZ_TCP_PROTOCOL     "ns3::TcpBbr"
//#define VZ_TCP_PROTOCOL     "ns3::TcpNewReno"

// For logging.  Note, to display, set environment variable:
NS_LOG_COMPONENT_DEFINE ("main");

/////////////////////////////////////////////////
int main (int argc, char *argv[]) {

  /////////////////////////////////////////
  // Setup environment
  LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
  Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                     StringValue(VZ_TCP_PROTOCOL));

  // Report parameters.
  NS_LOG_INFO("TCP protocol: " << VZ_TCP_PROTOCOL);
  NS_LOG_INFO("Server to Router Bwdth: " << VZ_S_TO_R_BW);
  NS_LOG_INFO("Server to Router Delay: " << VZ_S_TO_R_DELAY);
  NS_LOG_INFO("Router to Client Bwdth: " << VZ_R_TO_C_BW);
  NS_LOG_INFO("Router to Client Delay: " << VZ_R_TO_C_DELAY);
  NS_LOG_INFO("Packet size (bytes): " << VZ_PACKET_SIZE);
  
  // Set segment size (otherwise, default is 536).
  Config::SetDefault("ns3::TcpSocket::SegmentSize",
                     UintegerValue(VZ_PACKET_SIZE)); 

  // Turn off delayed ack (so, acks every packet).
  // Note, BBR' can still work without this.
  Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(0));
   
  /////////////////////////////////////////
  // Create nodes.
  NS_LOG_INFO("Creating nodes.");
  NodeContainer nodes;  // 0=source, 1=router, 2=sink
  nodes.Create(3);

  /////////////////////////////////////////
  // Create channels.
  NS_LOG_INFO("Creating channels.");
  NodeContainer n0_to_r = NodeContainer(nodes.Get(0), nodes.Get(1));
  NodeContainer r_to_n1 = NodeContainer(nodes.Get(1), nodes.Get(2));

  /////////////////////////////////////////
  // Create links.
  NS_LOG_INFO("Creating links.");

  // Server to Router.
  int mtu = 1500;
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue (VZ_S_TO_R_BW));
  p2p.SetChannelAttribute("Delay", StringValue (VZ_S_TO_R_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NetDeviceContainer devices1 = p2p.Install(n0_to_r);

  // Router to Client.
  p2p.SetDeviceAttribute("DataRate", StringValue (VZ_R_TO_C_BW));
  p2p.SetChannelAttribute("Delay", StringValue (VZ_R_TO_C_DELAY));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue(mtu));
  NS_LOG_INFO("Router queue size: "<< VZ_QUEUE_SIZE);
  p2p.SetQueue("ns3::DropTailQueue",
               "Mode", StringValue ("QUEUE_MODE_PACKETS"),
               "MaxPackets", UintegerValue(VZ_QUEUE_SIZE));
  NetDeviceContainer devices2 = p2p.Install(r_to_n1);

  /////////////////////////////////////////
  // Install Internet stack.
  InternetStackHelper internet;
  internet.Install(nodes);
  
  /////////////////////////////////////////
  // Add IP addresses.
  NS_LOG_INFO("Assigning IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign(devices1);

  ipv4.SetBase("191.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign(devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  /////////////////////////////////////////
  // Create apps.
  NS_LOG_INFO("Creating applications.");
  NS_LOG_INFO("  Bulk send.");

  // Well-known port for server.
  uint16_t port = 911;  

  // Source (at node 0).
  BulkSendHelper source("ns3::TcpSocketFactory",
                        InetSocketAddress(i1i2.GetAddress(1), port));
  // Set the amount of data to send in bytes (0 for unlimited).
  source.SetAttribute("MaxBytes", UintegerValue(0));
  source.SetAttribute("SendSize", UintegerValue(VZ_PACKET_SIZE));
  ApplicationContainer apps = source.Install(nodes.Get(0));
  apps.Start(Seconds(VZ_START_TIME));
  apps.Stop(Seconds(VZ_STOP_TIME));

  // Sink (at node 2).
  PacketSinkHelper sink("ns3::TcpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), port));
  apps = sink.Install(nodes.Get(2));
  apps.Start(Seconds(VZ_START_TIME));
  apps.Stop(Seconds(VZ_STOP_TIME));
  Ptr<PacketSink> p_sink = DynamicCast<PacketSink> (apps.Get(0)); // 4 stats

  /////////////////////////////////////////
  // Setup tracing (as appropriate).
  if (VZ_ENABLE_TRACE) {
    NS_LOG_INFO("Enabling trace files.");
    AsciiTraceHelper ath;
    p2p.EnableAsciiAll(ath.CreateFileStream("trace.tr"));
  }  
  if (VZ_ENABLE_PCAP) {
    NS_LOG_INFO("Enabling pcap file.");
    p2p.EnablePcapAll("shark", true);
  }

  /////////////////////////////////////////
  // Run simulation.
  Simulator::Stop(Seconds(VZ_STOP_TIME));
  NS_LOG_INFO("Simulation time: [" << 
              VZ_START_TIME << "," <<
              VZ_STOP_TIME << "]");
  NS_LOG_INFO("--------------------------------------------");
  Simulator::Run();
  NS_LOG_INFO("--------------------------------------------");

  /////////////////////////////////////////
  // Ouput stats.
  NS_LOG_INFO("Total bytes received: " << p_sink->GetTotalRx());
  double tput = p_sink->GetTotalRx() / (VZ_STOP_TIME - VZ_START_TIME);
  tput *= 8;          // Convert to bits.
  tput /= 1000000.0;  // Convert to Mb/s
  NS_LOG_INFO("Throughput: " << tput << " Mb/s");
  NS_LOG_INFO("Done.");

  // Done.
  Simulator::Destroy();
  return 0;
}
