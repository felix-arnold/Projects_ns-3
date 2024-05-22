#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

void empty_cell_test(std::vector<std::pair<double, double>> final_node_positions) {
    
    const double cell_size = 7.5;
    std::array<std::array<bool, 10>, 10> grid;
    grid.fill({});

    for (auto& position : final_node_positions) {
        grid[floor((position.first + 37.5)/ cell_size)][floor((position.second + 37.5)/ cell_size)] = true;
    }

    uint32_t empty_cells = 0;

    for (int i = 0; i<10; i++) {
        for (int j = 0; j<10; j++) {
            if (!grid[i][j]) 
                empty_cells ++;
        }
    }

    std::cout << empty_cells << std::endl;
}

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
    uint32_t nNodes = 4;
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

    RngSeedManager::SetSeed(time(0));

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
        pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
        pos.Set("X", StringValue("ns3::UniformRandomVariable[Min=-37.5|Max=37.5]"));
        pos.Set("Y", StringValue("ns3::UniformRandomVariable[Min=-37.5|Max=37.5]"));
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

    /*std::cout << "Initial Node Positions" << std::endl;
    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<MobilityModel> model = nodes.Get(i)->GetObject<MobilityModel>();
        Vector position = model->GetPosition();
        std::cout << position.x << "\t" << position.y << std::endl;
    }

    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<MobilityModel> model = nodes.Get(i)->GetObject<MobilityModel>();
        std::ostringstream oss;
        oss << "/NodeList/" << nodes.Get(i)->GetId() << "/$ns3::MobilityModel/CourseChange";
        Config::Connect(oss.str(), MakeCallback(&CourseChange));
    }*/

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    std::vector<std::pair<double, double>> final_node_positions;

    std::cout << "Final Node Positions" << std::endl;
    for (uint32_t i = 0; i < nNodes; i++) {
        Ptr<MobilityModel> model = nodes.Get(i)->GetObject<MobilityModel>();
        Vector position = model->GetPosition();
        float distance = sqrt(pow(position.x, 2) + pow(position.y, 2));
        std::cout << "Position X: " << position.x << "\tPosition Y: " << position.y << "\tDistance: " << distance << std::endl;        
        //final_node_positions.push_back(std::make_pair(position.x, position.y));
    }

    //empty_cell_test(final_node_positions);

    Simulator::Destroy();
}
