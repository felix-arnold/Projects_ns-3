#include "ns3/boolean.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/gnuplot.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab 4 Part 1");

float rate;

class NodeStatistics {
  public:
    NodeStatistics() = default;
    void AdvancePosition(Ptr<Node> staNode, Ptr<Node> apNode, float stepsSize, float stepsTime);

  private:
    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
};

void NodeStatistics::SetPosition(Ptr<Node> node, Vector position) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    mobility->SetPosition(position);
}

Vector NodeStatistics::GetPosition(Ptr<Node> node) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    return mobility->GetPosition();
}

void NodeStatistics::AdvancePosition(Ptr<Node> staNode, Ptr<Node> apNode, float stepsSize, float stepsTime) {
    Vector staPos = GetPosition(staNode);
    static Vector apPos = GetPosition(apNode);

    double distance = CalculateDistance(staPos, apPos);

    std::cout << "Current Simulation Time:\t\t" << Simulator::Now().GetSeconds() << " s" << std::endl;
    std::cout << "Station Location:\t\t\tx=" << staPos.x << ",\ty=" << staPos.y << ",\tz=" << staPos.z << std::endl;
    std::cout << "Distance between station and AP:\t" << distance << std::endl;
    std::cout << "Final link data rate:\t\t\t" << rate << " Mbps" << std::endl << std::endl;

    staPos.x += (staPos.x >= 65) ? 0 : stepsSize;
    staPos.y += (staPos.y >= 20) ? 0 : stepsSize;
    SetPosition(staNode, staPos);
    
    Simulator::Schedule(Seconds(stepsTime),
                        &NodeStatistics::AdvancePosition,
                        this,
                        staNode,
                        apNode,
                        stepsSize,
                        stepsTime);
}

void RateCallback(uint64_t oldRate, uint64_t newRate) {
    rate = newRate / 1000000.0;
}

int main (int argc, char *argv[]) {

    double pl_exp = 0.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("pl_exp", "Propagation loss exponent", pl_exp);
    cmd.Parse(argc, argv);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel", "Exponent", DoubleValue(pl_exp));
    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("ChannelSettings", StringValue ("{0, 20, BAND_UNSPECIFIED, 0}"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");

    NetDeviceContainer wifiStaDevice;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    wifiStaDevice = wifi.Install(wifiPhy, mac, wifiStaNode);

    NetDeviceContainer wifiApDevice;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    wifiApDevice = wifi.Install(wifiPhy, mac, wifiApNode);

    NetDeviceContainer wifiDevices;
    wifiDevices.Add(wifiStaDevice);
    wifiDevices.Add(wifiApDevice);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    positionAlloc->Add(Vector(0.0, 0.0, 4.0));
    positionAlloc->Add(Vector(10.0, 0.0, 1.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode.Get(0));
    mobility.Install(wifiStaNode.Get(0));

    NodeStatistics atpCounter = NodeStatistics();

    Simulator::Schedule(Seconds(2.0),
                        &NodeStatistics::AdvancePosition,
                        &atpCounter,
                        wifiStaNode.Get(0),
                        wifiApNode.Get(0),
                        5.0,
                        2.0);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = address.Assign(wifiDevices);
    Ipv4Address sinkAddress = i.GetAddress(0);
    uint16_t port = 9;

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(sinkAddress, port));
    ApplicationContainer apps_sink = sink.Install(wifiStaNode.Get(0));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(sinkAddress, port));
    onoff.SetConstantRate(DataRate("400Mb/s"), 1024);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(20.0)));
    ApplicationContainer apps_source = onoff.Install(wifiApNode.Get(0));

    Config::ConnectWithoutContextFailSafe(
        "/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/$ns3::MinstrelHtWifiManager/Rate",
        MakeCallback(RateCallback));
    Simulator::Stop(Seconds(26.0));
    
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}
