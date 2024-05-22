#include "manet_routing_simulation.hh"

bool test1=false;
uint64_t txfirst;
uint64_t rxlast;

NS_LOG_COMPONENT_DEFINE("manet-routing-compare");

// Callback function to display mobility model course change (Commented call currently)
void CourseChange (std::string context, Ptr<const MobilityModel> model) {
  Vector position = model->GetPosition ();
  NS_LOG_UNCOND (Simulator::Now().GetSeconds() << ": " << context << " x = " << position.x << ", y = " << position.y << ", z = " << position.z);
}

// Get a list of active nodes at the start of simulation
std::vector<uint32_t> Ns2NodeUtility::GetActiveNodesAtStart(uint32_t nodeId) {
  std::vector<uint32_t> activeNodesList = {};
  double startTime = GetEntryTimeForNode(nodeId);
  for (const auto& node : m_node_times) {
    if (node.second.first < startTime && node.second.second > startTime) {
      activeNodesList.push_back(node.first);
    }
  }
  return activeNodesList;
}

// Callback function fall all packets transmision
void RoutingExperiment::TxTrace(std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface) {
  stat.numTxTot++;
}

// Legacy function, No used anymore as it does not take into account multiple reception of a single packet
void RoutingExperiment::RxTrace(std::string context, Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface) {
  if (stat.pkt_timings.find(packet->GetUid()) == stat.pkt_timings.end()) {
      std::cout << packet->GetUid() << std::endl;
    stat.numRxOverheadPkt++;
    stat.numRxOverheadBytes += packet->GetSize();
  }
}

// Constructor for Ns2NodeUtility class
Ns2NodeUtility::Ns2NodeUtility(std::string name) {
  m_file_name = name;
  m_input_file.open(m_file_name);

  std::string line;
  std::vector <uint32_t> node_ids;

  while (std::getline (m_input_file, line)) {
	  std::smatch sm;
  	std::regex r("\\$ns_ at (\\d*.\\d) \"\\$node_\\((\\d*)\\)");
	  if (std::regex_search(line, sm, r)) {
	    uint32_t id = std::stoi(sm[2]);
	    double new_latest = std::stof(sm[1]);

	    if ( std::find (node_ids.begin(), node_ids.end(), id) != node_ids.end()) {
  		  double entry_time = std::get<0> (m_node_times [id]);
	    	m_node_times [id] = std::make_pair(entry_time, new_latest);
      }
	    else
    		m_node_times [id] = std::make_pair(new_latest, new_latest);
	    node_ids.push_back(id);
	  }
  }
}

// Function to get the total number of nodes in the simulation
uint32_t Ns2NodeUtility::GetNNodes() {
  return m_node_times.size();
}

// Function to get entry time for a node in the network
double Ns2NodeUtility::GetEntryTimeForNode(uint32_t nodeId) {
  return std::get<0>(m_node_times [nodeId]);
}

// Function to get exit time for a node in the network
double Ns2NodeUtility::GetExitTimeForNode(uint32_t nodeId) {
  return std::get<1>(m_node_times [nodeId]);
}

// Function to get simulation time
double Ns2NodeUtility::GetSimulationTime() {
  double time = 0;

  for (uint32_t i=0 ; i<m_node_times.size(); i++)
    time = std::max ( time,  std::get<1>(m_node_times[i]) );

  return time;
}

// Print information about node start and end times
void Ns2NodeUtility::PrintInformation() {
  uint32_t s = m_node_times.size();
  for (uint32_t i=0 ; i<s; i++)
  	std::cout << "Node " << i << " started " << std::get<0>(m_node_times[i]) << " and ended " << std::get<1>(m_node_times[i]) << std::endl;
}

RoutingExperiment::RoutingExperiment() = default;

// Exit the network for a given node
void RoutingExperiment::ExitNetwork(uint32_t nodeId) {
  std::pair<Ptr<Ipv4>, uint32_t> returnValue = adhocInterfaces.Get(nodeId);
  Ptr<Ipv4> ipv4 = returnValue.first;
  uint32_t index = returnValue.second;
  Ptr<Ipv4Interface> iface =  ipv4->GetObject<Ipv4L3Protocol>()->GetInterface (index);
  //NS_LOG_UNCOND (Simulator::Now().GetSeconds() << "s Set " << iface->GetAddress(0).GetLocal() << " down.");
  ipv4->SetDown(index);
}

// Setup command line arguments
void RoutingExperiment::CommandSetup(int argc, char** argv) {
  CommandLine cmd(__FILE__);
  cmd.AddValue("protocol", "Routing protocol (OSLR, AODV, DSDV, DSR)", m_protocolName);
  cmd.AddValue("SUMOtraceFile", "SUMO input file", m_SUMOtraceFile);
  cmd.Parse(argc, argv);

  std::vector<std::string> allowedProtocols{"OLSR", "AODV", "DSDV", "DSR"};

  if (std::find(std::begin(allowedProtocols), std::end(allowedProtocols), m_protocolName) == std::end(allowedProtocols)) {
      NS_FATAL_ERROR("No such protocol:" << m_protocolName);
  }
}

// Callback function for data packet transmission
void TxCallback(stat_t *stat, std::string context, Ptr<const Packet> p) {
  //std::cout << "A packet is sent from " << context << " at time " << Simulator::Now().GetSeconds() << std::endl;
  if (!test1) {
    txfirst = Simulator::Now().GetMicroSeconds();
    test1=true;
  }
  stat->pkt_timings[p->GetUid()].first = Simulator::Now().GetMicroSeconds();
  stat->numTxDataPkt++;
}

// Check throughput at regular intervals
void RoutingExperiment::CheckThroughput() {
  double kbs = (bytesTotal * 8.0) / 1000;
  bytesTotal = 0;

  std::cout << (Simulator::Now()).GetSeconds() << "," << kbs << "," << packetsReceived << "," << m_protocolName << "," << m_txp << "" << std::endl;

  packetsReceived = 0;
  Simulator::Schedule(Seconds(1.0), &RoutingExperiment::CheckThroughput, this);
}

// Main function to run the simulation
void RoutingExperiment::Run() {

  Packet::EnablePrinting();

  std::string rate("2048bps");
  std::string phyMode("OfdmRate6MbpsBW10MHz");
  std::string tr_name("manet-routing-compare");

  Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("512"));
  Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue(rate));
  Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

  SeedManager::SetSeed(123456789);
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();

  Ns2NodeUtility ns2_utility(m_SUMOtraceFile);
  nWifis = ns2_utility.GetNNodes();
  adhocNodes.Create(nWifis);
  NS_LOG_UNCOND("There are " << nWifis << " vehicles in the simulation.\n");

  TotalTime = ns2_utility.GetSimulationTime();
  NS_LOG_UNCOND("The total simulation time is " << TotalTime << " seconds.\n");

  Ns2MobilityHelper sumo_trace(m_SUMOtraceFile);
  sumo_trace.Install();

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211p);

  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

  wifiPhy.Set("TxPowerStart", DoubleValue(m_txp));
  wifiPhy.Set("TxPowerEnd", DoubleValue(m_txp));

  wifiMac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer adhocDevices = wifi.Install(wifiPhy, wifiMac, adhocNodes);

  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  DsrMainHelper dsrMain;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  if (m_protocolName == "OLSR") {
    list.Add(olsr, 100);
    internet.SetRoutingHelper(list);
    internet.Install(adhocNodes);
  }
  else if (m_protocolName == "AODV") {
    list.Add(aodv, 100);
    internet.SetRoutingHelper(list);
    internet.Install(adhocNodes);
  }
  else if (m_protocolName == "DSDV") {
    list.Add(dsdv, 100);
    internet.SetRoutingHelper(list);
    internet.Install(adhocNodes);
  }
  else if (m_protocolName == "DSR") {
    internet.Install(adhocNodes);
    dsrMain.Install(dsr, adhocNodes);
  }

  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase("10.1.1.0", "255.255.255.0");
  adhocInterfaces = addressAdhoc.Assign(adhocDevices);

  OnOffHelper onoff1("ns3::UdpSocketFactory", Address());
  onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

  for (uint32_t i = 0; i < nWifis; i++) {
    Ptr<Socket> sink = SetupPacketReceive(adhocInterfaces.GetAddress(i), adhocNodes.Get(i));
    //std::cout << i << " starting at " << ns2_utility.GetEntryTimeForNode(i) << " ending at " << ns2_utility.GetExitTimeForNode(i) << std::endl;

    std::vector<uint32_t> activeNodes = ns2_utility.GetActiveNodesAtStart(i);
    if (!activeNodes.empty()) {
      rand->SetAttribute("Min", DoubleValue(0.0));
      rand->SetAttribute("Max", DoubleValue(activeNodes.size()));
      uint32_t remoteId = activeNodes[floor(rand->GetValue())];

      AddressValue remoteAddress(InetSocketAddress(adhocInterfaces.GetAddress(remoteId), port));
      onoff1.SetAttribute("Remote", remoteAddress);

      ApplicationContainer temp = onoff1.Install(adhocNodes.Get(i));
      temp.Start(Seconds(ns2_utility.GetEntryTimeForNode(i)));
      temp.Stop(Seconds(Min(ns2_utility.GetExitTimeForNode(remoteId), ns2_utility.GetExitTimeForNode(i))));  
      
      std::string context = "/NodeList/" + std::to_string(i) + "/ApplicationList/*/$ns3::OnOffApplication/Tx";
      Config::Connect(context, MakeBoundCallback(&TxCallback, &stat));
    }

    Simulator::Schedule(Seconds(ns2_utility.GetExitTimeForNode(i)), [this, i] {
      ExitNetwork(i);
    });
  }

  //Config::Connect("/NodeList/1/$ns3::MobilityModel/CourseChange", MakeCallback(&CourseChange));
  CheckThroughput();

  Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/Tx", MakeCallback(&RoutingExperiment::TxTrace, this));
  //Config::Connect("/NodeList/*/$ns3::Ipv4L3Protocol/Rx", MakeCallback(&RoutingExperiment::RxTrace, this));

  NS_LOG_UNCOND("Run Simulation.");
  Simulator::Stop(Seconds(TotalTime));
  Simulator::Run();
  
  std::cout << std::endl << std::endl << "  Total Tx Data Packets: " << stat.numTxDataPkt << std::endl;
  std::cout << "  Total Rx Data Packets: " << stat.numRxDataPkt << std::endl;
  std::cout << "  Total Bytes Received: " << stat.numRxDataBytes << std::endl;;
  std::cout << "  Throughput: " << (stat.numRxDataBytes * 8.0/(rxlast-txfirst)*1000000)/1000 << " Kbps" << std::endl;;
  std::cout << "  Packets Delivery Ratio: " << (stat.numRxDataPkt * 100 / stat.numTxDataPkt) << "%" << std::endl;;

  double endToEndDelay = 0;
  for (auto pktInfo : stat.pkt_timings) {
    if (pktInfo.second.second) {
      endToEndDelay += pktInfo.second.second - pktInfo.second.first;
    }
  }
  
  std::cout << "  Average End to End Delay: " << endToEndDelay/1000000/stat.numRxDataPkt << std::endl;
  std::cout << "  Total Tx Packet: " << stat.numTxTot << std::endl;
  std::cout << "  Overhead Ratio: " << double(stat.numTxTot - stat.numTxDataPkt)/stat.numTxTot*100 << std::endl;

  Simulator::Destroy();
}

// Print information about received data packet (call commented currently)
static inline std::string PrintReceivedPacket(Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress) {
  std::ostringstream oss;

  oss << Simulator::Now().GetSeconds() << " " << socket->GetNode()->GetId();

  if (InetSocketAddress::IsMatchingType(senderAddress)) {
    InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderAddress);
    oss << " received one packet from " << addr.GetIpv4();
  }
  else
    oss << " received one packet!";
  return oss.str();
}

// Receive data packets on a socket
void RoutingExperiment::ReceivePacket(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address senderAddress;
  while ((packet = socket->RecvFrom(senderAddress))) {
    stat.numRxDataBytes += packet->GetSize();
    bytesTotal += packet->GetSize();
    stat.numRxDataPkt++;
    packetsReceived++;
    stat.pkt_timings[packet->GetUid()].second = Simulator::Now().GetMicroSeconds();
    rxlast = Simulator::Now().GetMicroSeconds();
    //NS_LOG_UNCOND(PrintReceivedPacket(socket, packet, senderAddress, &stat));
  }
}

// Setup packet reception on a node
Ptr<Socket> RoutingExperiment::SetupPacketReceive(Ipv4Address addr, Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sink = Socket::CreateSocket(node, tid);
  InetSocketAddress local = InetSocketAddress(addr, port);
  sink->Bind(local);
  sink->SetRecvCallback(MakeCallback(&RoutingExperiment::ReceivePacket, this));

  return sink;
}

// Main function
int main(int argc, char* argv[]) {
  RoutingExperiment experiment;
  experiment.CommandSetup(argc, argv);
  experiment.Run();

  return 0;
}
