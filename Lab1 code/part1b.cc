#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-apps-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Part 1b");

// Define the parameters of the simulation and set their default value
uint32_t nRelays = 1;
float interval = 1.0;
std::string dataRate = "5Mbps";
std::string delay = "2ms";

int main(int argc, char* argv[]) {

    // Set the seed of the random generator
    RngSeedManager::SetSeed(123456789);

    // Define the random generator and set its attributes
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
    rand->SetAttribute ("Min", DoubleValue (2));
    rand->SetAttribute ("Max", DoubleValue (6));

    // Define and parse command line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("nRelays", "Number of relay between client 1 and the server", nRelays);
    cmd.AddValue("interval", "Interval in seconds between 2 packets sent by the same client", interval);
    cmd.AddValue("dataRate", "Data rate of point-to-point channels", dataRate);
    cmd.AddValue("delay", "Propagation delay of point to point channels", delay);
    cmd.Parse(argc, argv);
    
    // Enable Logging
    LogComponentEnable("Ping", LOG_LEVEL_INFO);

    // Set the time resolution in nanoseconds
    Time::SetResolution(Time::NS);

    // Create the list of nodes from the topology
    NodeContainer nodes;
    nodes.Create(nRelays+3);

    // Create the Point-to-Point connections and net devices
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate)); 
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay)); 

    // Install net devices and save them in devices vector
    std::vector<NetDeviceContainer> devices;
    for (uint32_t i = 0; i < nRelays+2; i++)
        devices.push_back(pointToPoint.Install(nodes.Get(i), nodes.Get(i+1)));

    // Install Internet stack on nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to interfaces
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;
    for (uint32_t i = 0; i < nRelays+2; i++) {
        address.SetBase(("10.1." + std::to_string(i+1) + std::string(".0")).c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(devices[i]));
    }

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Set up the Ping application
    PingHelper pingClient(interfaces[0].GetAddress(1));
    pingClient.SetAttribute("Interval", TimeValue(Seconds(interval)));

    // Install Ping application on client 1
    ApplicationContainer clientApps1 = pingClient.Install(nodes.Get(nRelays+2));
    clientApps1.Start(Seconds(rand->GetValue()));
    clientApps1.Stop(Seconds(20.0));

    // Install Ping application on client 0
    ApplicationContainer clientApps2 = pingClient.Install(nodes.Get(0));
    clientApps2.Start(Seconds(rand->GetValue()));
    clientApps2.Stop(Seconds(20.0));

    // Enable PCAP tracing for all Point-to-Point links
    pointToPoint.EnablePcapAll("trace_part1b");
    
    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
