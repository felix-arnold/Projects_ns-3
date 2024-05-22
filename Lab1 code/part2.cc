#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-apps-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Part 2");

// Define the parameters of the simulation and set their default value
uint32_t nRelays = 1;
uint32_t nCsma = 3;
float interval = 5.0;

int main(int argc, char* argv[]) {

    // Define and parse command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("nRelays", "Number of relay between client 1 and the server", nRelays);
    cmd.AddValue("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
    cmd.AddValue("interval", "Interval in seconds between 2 packets sent by the same client", interval);
    cmd.Parse(argc, argv);

    // Enable Logging
    LogComponentEnable("Ping", LOG_LEVEL_INFO);
    LogComponentEnable("ArpL3Protocol", LOG_LEVEL_FUNCTION);
    
    // Set the time resolution in nanoseconds
    Time::SetResolution(Time::NS);
    
    //Check that there will be at least 2 Csma nodes
    nCsma = nCsma == 0 ? 1 : nCsma;

    // Create the list of nodes from the topology
    NodeContainer p2pNodes;
    p2pNodes.Create(nRelays+2);

    NodeContainer csmaNodes;
    csmaNodes.Add(p2pNodes.Get(nRelays+1));
    csmaNodes.Create(nCsma);

    // Create the Point-to-Point and CSMA connections and net devices
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    // Install net devices and save them in devices vector
    std::vector<NetDeviceContainer> p2pDevices;
    for (uint32_t i = 0; i < nRelays+1; i++)
        p2pDevices.push_back(pointToPoint.Install(p2pNodes.Get(i), p2pNodes.Get(i+1)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install(csmaNodes);

    // Install Internet stack on nodes
    InternetStackHelper stack;
    stack.Install(p2pNodes);
    stack.Install(csmaNodes);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> p2pInterfaces;
    for (uint32_t i = 0; i < nRelays+1; i++) {
        address.SetBase(("10.1." + std::to_string(nRelays-i+2) + std::string(".0")).c_str(), "255.255.255.0");
        p2pInterfaces.push_back(address.Assign(p2pDevices[i]));
    }

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign(csmaDevices);

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Set up the Ping application
    PingHelper pingClient(csmaInterfaces.GetAddress(nCsma));
    pingClient.SetAttribute("Interval", TimeValue(Seconds(interval)));

    // Install Ping application on client
    ApplicationContainer clientApps = pingClient.Install(p2pNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Enable PCAP tracing
    pointToPoint.EnablePcapAll("trace_part2");
    csma.EnablePcap("trace_part2", csmaDevices.Get(1), true);

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
