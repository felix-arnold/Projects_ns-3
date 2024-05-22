#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/olsr-module.h"
#include "ns3/yans-wifi-helper.h"

#include "ns2-node-utility.h"
#include <regex>
#include "custom-data-tag.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <fstream>
#include <iostream>

using namespace ns3;
using namespace dsr;

typedef struct {
  uint32_t numTxDataPkt = 0;
  uint32_t numRxDataPkt = 0;
  uint32_t numRxDataBytes = 0;
  uint32_t numTxTot = 0;
  uint32_t numRxOverheadPkt = 0;
  uint32_t numRxOverheadBytes = 0;
  std::unordered_map<uint64_t, std::pair<uint64_t, uint64_t>> pkt_timings;
} stat_t;

class RoutingExperiment {
  
  public:
    RoutingExperiment();
    void Run();
    void CommandSetup(int argc, char** argv);
    void RxTrace(std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface);
    void TxTrace(std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface);

  private:
    Ptr<Socket> SetupPacketReceive(Ipv4Address addr, Ptr<Node> node);
    void ReceivePacket(Ptr<Socket> socket);
    void CheckThroughput();
    void ExitNetwork(uint32_t nodeId);

    NodeContainer adhocNodes;
    Ipv4InterfaceContainer adhocInterfaces;

    stat_t stat;

    uint32_t nWifis;
    double TotalTime;

    uint32_t port{9};            // Receiving port number.
    uint32_t bytesTotal{0};      // Total received bytes.
    uint32_t packetsReceived{0}; // Total received packets.

    std::string m_protocolName{"DSR"};                      // Protocol name.
    double m_txp{10};                                       // Tx power.
    std::string m_SUMOtraceFile{"scratch/ns2mobility.tcl"}; // SUMO trace file.

    std::vector<uint32_t> activeNodes;
};
