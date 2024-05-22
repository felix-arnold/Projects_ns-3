#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/enum.h"
#include "ns3/error-model.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-header.h"
#include "ns3/traffic-control-module.h"
#include "ns3/udp-header.h"

#include <inttypes.h>
#include <fstream>
#include <iostream>
#include <string>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Lab 2 Part 1");

enum delay_set { SHORT = 0, LONG = 1};

static bool firstCwnd = true;
static bool firstSshThr = true;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> cWndStream;
static std::map<uint32_t, Ptr<OutputStreamWrapper>> ssThreshStream;
static std::map<uint32_t, uint32_t> cWndValue;
static std::map<uint32_t, uint32_t> ssThreshValue;

static uint32_t GetNodeIdFromContext(std::string context) {
    const std::size_t n1 = context.find_first_of('/', 1);
    const std::size_t n2 = context.find_first_of('/', n1 + 1);
    return std::stoul(context.substr(n1 + 1, n2 - n1 - 1));
}

static void CwndTracer(std::string context, uint32_t oldval, uint32_t newval) {
    uint32_t nodeId = GetNodeIdFromContext(context);

    if (firstCwnd) {
        *cWndStream[nodeId]->GetStream() << "0.0 " << oldval << std::endl;
        firstCwnd = false;
    }
    *cWndStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    cWndValue[nodeId] = newval;

    if (!firstSshThr)
        *ssThreshStream[nodeId]->GetStream() << Simulator::Now().GetSeconds() << " " << ssThreshValue[nodeId] << std::endl;
}

static void TraceCwnd(std::string cwnd_tr_file_name, uint32_t nodeId) {
    AsciiTraceHelper ascii;
    cWndStream[nodeId] = ascii.CreateFileStream(cwnd_tr_file_name);
    Config::Connect("/NodeList/" + std::to_string(nodeId) + "/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                    MakeCallback(&CwndTracer));
}

int main(int argc, char* argv[]) {
    
    std::string data_rate = "1Mbps";  
    std::string delay = "20ms"; 
    double error_rate = 0.00001; 
    uint16_t num_flows = 2;  
    std::string transport_prot = "TcpNewReno"; 
    uint16_t run = 0;

    CommandLine cmd;
    cmd.AddValue("dataRate", "Data rate of the bottleneck link", data_rate);
    cmd.AddValue("delay", "Delay of the bottleneck link", delay);
    cmd.AddValue("errorRate", "Error rate of the bottleneck link", error_rate);
    cmd.AddValue("nFlows", "Number of flows", num_flows);
    cmd.AddValue("transport_prot", "Transport protocol", transport_prot);
    cmd.AddValue("run", "Run number", run);
    cmd.Parse(argc, argv);

    std::string file_name = transport_prot + "_" + data_rate + "_" + delay + "_" + std::to_string(num_flows);

    transport_prot = std::string("ns3::") + transport_prot;

    uint64_t data_mbytes = 0;

    SeedManager::SetSeed(123456789);
    SeedManager::SetRun(run);

    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    rand->SetAttribute("Min", DoubleValue(1.0));
    rand->SetAttribute("Max", DoubleValue(1.5));

    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(transport_prot, &tcpTid), "TypeId " << transport_prot << " not found");
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(transport_prot)));

    NodeContainer sources;
    sources.Create(2);
    NodeContainer gateways;
    gateways.Create(2);
    NodeContainer sinks;
    sinks.Create(2);

    InternetStackHelper stack;
    stack.InstallAll();

    PointToPointHelper LocalLinkHighDelay;
    LocalLinkHighDelay.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    LocalLinkHighDelay.SetChannelAttribute("Delay", StringValue("0.01ms"));

    PointToPointHelper LocalLinkLowDelay;
    LocalLinkLowDelay.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    LocalLinkLowDelay.SetChannelAttribute("Delay", StringValue("100ms"));

    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetStream(50);
    RateErrorModel error_model;
    error_model.SetRandomVariable(uv);
    error_model.SetUnit(RateErrorModel::ERROR_UNIT_BYTE);
    error_model.SetRate(error_rate);

    PointToPointHelper UnReLink;
    UnReLink.SetDeviceAttribute("DataRate", StringValue(data_rate));
    UnReLink.SetChannelAttribute("Delay", StringValue(delay));
    UnReLink.SetDeviceAttribute("ReceiveErrorModel", PointerValue(&error_model));

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");

    std::vector<NetDeviceContainer> devices;
    std::vector<Ipv4InterfaceContainer> interfaces;

    devices.push_back(LocalLinkHighDelay.Install(sources.Get(0), gateways.Get(0)));
    address.NewNetwork();
    interfaces.push_back(address.Assign(devices[0]));

    devices.push_back(LocalLinkLowDelay.Install(sources.Get(1), gateways.Get(0)));
    address.NewNetwork();
    interfaces.push_back(address.Assign(devices[1]));

    devices.push_back(UnReLink.Install(gateways.Get(0), gateways.Get(1)));
    address.NewNetwork();
    interfaces.push_back(address.Assign(devices[2]));

    devices.push_back(LocalLinkHighDelay.Install(gateways.Get(1), sinks.Get(0)));
    address.NewNetwork();
    interfaces.push_back(address.Assign(devices[3]));

    devices.push_back(LocalLinkLowDelay.Install(gateways.Get(1), sinks.Get(1)));
    address.NewNetwork();
    interfaces.push_back(address.Assign(devices[4]));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    std::vector<double> start_times;
    std::vector<ApplicationContainer> sinkApps;
    std::vector<std::pair<delay_set, delay_set>> flow_set;

    for (uint32_t i = 0; i < 2*num_flows; i++) {
        BulkSendHelper sourceHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces[3 + i%2].GetAddress(1), port+i));
        sourceHelper.SetAttribute("MaxBytes", UintegerValue(data_mbytes));
        ApplicationContainer sourceApps = sourceHelper.Install(sources.Get((i < num_flows) ? 0 : 1));
        start_times.push_back(rand->GetValue());
        sourceApps.Start(Seconds(start_times[i]));
        sourceApps.Stop(Seconds(20.0));

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port+i));
        sinkApps.push_back(sinkHelper.Install(sinks.Get(i%2)));
        sinkApps[i].Start(Seconds(0.0));
        sinkApps[i].Stop(Seconds(20.0));

        flow_set.push_back(std::make_pair((i < num_flows) ? SHORT : LONG, (i%2) ? LONG : SHORT));
    }

    Simulator::Schedule(Seconds(start_times[0] + 0.00001),
                        &TraceCwnd,
                        file_name + "cwnd",
                        sources.Get(0)->GetId());

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    std::array<uint64_t, 3> goodputs = {0, 0, 0};
    for (uint32_t i = 0; i < 2*num_flows; i++) {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps[i].Get(0));
        uint64_t totalRx = sink->GetTotalRx();
        goodputs.at(flow_set[i].first + flow_set[i].second) +=  totalRx / (20.0 - start_times[i]);
    }
    goodputs[1] /= 2;
    std::cout << "Total Goodput short-short:" << goodputs[0] << std::endl << "Total Goodput short-long:" << goodputs[1] << std::endl << "Total Goodput long-long:" << goodputs[2] << std::endl;

    Simulator::Destroy();

    return 0;
}
