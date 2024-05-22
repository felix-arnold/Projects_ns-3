#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void CourseChange(std::string context, Ptr<const MobilityModel> model) {
    static bool firstTime = true;
    static double lastX, lastY;
    static double lastSpeed;
    double distance;

    Vector position = model->GetPosition ();
    Vector velocity = model->GetVelocity();

    if (!firstTime) {
        distance = sqrt((position.x - lastX)*(position.x - lastX)+(position.y - lastY)*(position.y - lastY));
        NS_LOG_UNCOND(Simulator::Now() << ":\t" << context << "\tx = " << position.x << ",\ty = " << position.y << ";\ttraveled a distance of " << distance << " meter(s) at a speed of " << lastSpeed << " m/s");
    }
    else
        NS_LOG_UNCOND(Simulator::Now() << ": " << context << " x = " << position.x << ", y = " << position.y);

    lastX = position.x;
    lastY = position.y;
    lastSpeed = sqrt(velocity.x*velocity.x + velocity.y*velocity.y);
    firstTime = false;
}

int main(int argc, char* argv[]) {

    std::string mobilityType = "walk";
    uint32_t nNodes = 1;
    double duration = 10.0;
    double minSpeed = 0.0;
    double maxSpeed = 1.0;
    double pause = 1.0;

    CommandLine cmd;
    cmd.AddValue ("mobilityType", "The type of mobility model to use (walk, way, or direction)", mobilityType);
    cmd.AddValue ("nNodes", "The number of nodes to simulate", nNodes);
    cmd.AddValue ("duration", "The duration of the simulation in seconds", duration);
    cmd.AddValue ("minSpeed", "The minimum node speed in m/s", minSpeed);
    cmd.AddValue ("maxSpeed", "The maximum node speed in m/s", maxSpeed);
    cmd.AddValue ("pause", "The pause time between movements in seconds (for way and direction models)", pause);
    cmd.Parse (argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    MobilityHelper mobility;

    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    rand->SetAttribute("Min", DoubleValue(-37.5));
    rand->SetAttribute("Max", DoubleValue(37.5));

    if (mobilityType == "walk") {
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue("Distance"),
                                  "Distance", DoubleValue(5.0),
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minSpeed) + "|Max=" + std::to_string(maxSpeed) + "]"),
                                  "Bounds", RectangleValue(Rectangle(-37.5, 37.5, -37.5, 37.5)));
    }
    else if (mobilityType == "way") {
        ObjectFactory pos;
        pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
        pos.Set ("X", StringValue("ns3::UniformRandomVariable[Min=-37.5|Max=37.5]"));
        pos.Set ("Y", StringValue("ns3::UniformRandomVariable[Min=-37.5|Max=37.5]"));
        Ptr<PositionAllocator> nodePositionAlloc = pos.Create()->GetObject<PositionAllocator>();

        mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minSpeed) + "|Max=" + std::to_string(maxSpeed) + "]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(pause) + "]"),
                                  "PositionAllocator", PointerValue(nodePositionAlloc));
    }
    else if (mobilityType == "direction") {
        mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                                  "Speed", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minSpeed) + "|Max=" + std::to_string(maxSpeed) + "]"),
                                  "Pause", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(pause) + "]"),
                                  "Bounds", RectangleValue(Rectangle(-37.5, 37.5, -37.5, 37.5)));
    }
    else
        NS_FATAL_ERROR ("Invalid mobility type: " << mobilityType);

    mobility.Install(nodes);

    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<MobilityModel> model = nodes.Get(i)->GetObject<MobilityModel>();
        Vector position = Vector(rand->GetValue(), rand->GetValue(), 0.0);
        model->SetPosition(position);
    }

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    double totalSpeed = 0.0;
    uint32_t activeNodes = 0;
    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<MobilityModel> model = nodes.Get(i)->GetObject<MobilityModel>();
        Vector position = model->GetPosition();
        Vector velocity = model->GetVelocity();
        double speed = sqrt(velocity.x*velocity.x + velocity.y*velocity.y);
        std::cout << "Position X: " << position.x << "\tPosition Y: " << position.y << "\tSpeed: " << speed << std::endl;        
        if (speed > 0) {
            totalSpeed += speed;
            activeNodes++;
        }
    }

    std::cout << "Average final node speed: " << totalSpeed/activeNodes << std::endl;

    Simulator::Destroy ();
}