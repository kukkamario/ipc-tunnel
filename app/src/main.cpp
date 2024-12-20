#include "comm.hpp"
#include "openamp.hpp"
#include "globaltimer.hpp"
#include "shared_state.h"
#include <iostream>
#include <thread>
#include "StatsProcessing.hpp"
#include "t0dataprocess.hpp"
#include <atomic>
#include <cmath>

#define ITERATION_LIMIT (20000 * 10)  // ~10s

static void Test(CommInterface& comm);
static void TestShm(CommInterface& comm);

static StatsProcessing t0Stats{ITERATION_LIMIT * 11 / 10}, t1Stats{ITERATION_LIMIT / 3}, t2Stats{ITERATION_LIMIT / 18};

static std::atomic_bool f_running{true};

static void T0Thread(CommInterface& comm);
static void T0ThreadShm(CommInterface& comm);

static bool f_useShm = false;
static std::string f_nameSuffix;

static bool f_withWorkload = false;

static void Workload() {
    static volatile double s_workStuff = 1.0;
    
    double var = s_workStuff;
    for (int i = 0; i < 1500; ++i) {
        var += 123.0;
        

        if (var > 1512436.0) {
            var = 123.0 - i;
        }
        
        var = std::cos(var) * 0.65 * s_workStuff;
    }
    
    if (var > -32165132.0 && var < 1234239051597.0) {
        s_workStuff = var;
    }
    else {
        s_workStuff = 634.0;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Expecting 2 parameters";
        return 1;
    }
    
#ifdef IPC_TUNNEL_CACHED
    f_nameSuffix += "-cached";
#endif
    
    if (strcmp(argv[2], "shm") == 0 || strcmp(argv[2], "shm_w") == 0) {
        f_useShm = true;
        f_nameSuffix += "-shm";
        std::cerr << "Transferring T0 variable data using shared memory" << std::endl;
    }
    
    if (strcmp(argv[2], "shm_w") == 0 || strcmp(argv[2], "w") == 0) {
        f_withWorkload = true;
        f_nameSuffix += "-work";
        std::cerr << "Running workload between T0 cycles" << std::endl;
    }
    
    auto comm = CreateFromArgs(argc, argv);
    if (!comm) {
        return 1;
    }
    
    comm->Initialize(!f_useShm);
    
    if (f_useShm) {
        if (!comm->MapT0SharedMemory()) {
            std::cerr << "Requested to use shared memory but it can't be mmapped" << std::endl;
            return 1;
        }
        
        std::thread t0Thread([&](){
            T0ThreadShm(*comm);
        });
        
        TestShm(*comm);
        t0Thread.join();
    }
    else {
        std::thread t0Thread([&](){
            T0Thread(*comm);
        });
        
        Test(*comm);
        t0Thread.join();
    }

    t0Stats.WriteCSV("benchmark-main-" + comm->GetInterfaceName() + f_nameSuffix + "-t0.csv");
    t1Stats.WriteCSV("benchmark-main-" + comm->GetInterfaceName() + f_nameSuffix + "-t1.csv");
    t2Stats.WriteCSV("benchmark-main-" + comm->GetInterfaceName() + f_nameSuffix + "-t2.csv");
    return 0;
}

static void T0Thread(CommInterface& comm) {
    T0DataProcess t0DataProcess(comm, ITERATION_LIMIT * 10ull / 9);
    uint8_t buf[0x1500];
    
    // Send start packet so the actual scheduling starts.
    // This is to avoid baremetal side filling ring buffers and dropping packets before
    // Linux side starts
    while (!comm.Send(Target::T0, buf, 1)) {}
    
    for (int i = 0; i < ITERATION_LIMIT; ++i) {
        size_t receivedBytes = comm.ReceiveT0(buf, sizeof(buf));
        auto receiveTime = global_timer::now();

        const SharedState_T0DataPacket* packet = reinterpret_cast<const SharedState_T0DataPacket*>(buf);
        auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
        t0DataProcess.HandleNewVariableData(packet->variables);
        t0Stats.AddReceivePacketLatency(receiveTime - sendTime);
        t0Stats.Add(packet->stats);
        
        auto workStart = global_timer::now();
        if (f_withWorkload) Workload();
        if (i & 1) t0DataProcess.SendRandomVariableUpdate();
        auto workEnd = global_timer::now();
        t0DataProcess.AddWorkDuration(workEnd - workStart);
    }
    
    f_running = false;
    std::cerr << "T0 thread finished. Writing results" << std::endl;
    t0DataProcess.WriteCSV("benchmark-main-" + comm.GetInterfaceName() + f_nameSuffix + "-variable-update.csv");

    t0DataProcess.SendShutdownCommand();
}

static void T0ThreadShm(CommInterface& comm) {
    T0DataProcess t0DataProcess(comm, ITERATION_LIMIT * 10ull / 9);
    
    
    uint8_t dummyPacket;
    // Send start packet so the actual scheduling starts.
    // This is to avoid baremetal side filling ring buffers and dropping packets before
    // Linux side starts
    while (!comm.Send(Target::T0, &dummyPacket, 1)) {}

    for (int i = 0; i < ITERATION_LIMIT; ++i) {
        
        while (!t0DataProcess.UpdateVariablesFromShm()) {
            // Polling shared memory
        }
        auto workStart = global_timer::now();

        if (f_withWorkload) Workload();
        if (i & 1) t0DataProcess.SendRandomVariableUpdate();
        
        auto workEnd = global_timer::now();
        t0DataProcess.AddWorkDuration(workEnd - workStart);
    }
    
    f_running = false;
    std::cerr << "T0 thread finished. Writing results" << std::endl;
    t0DataProcess.WriteCSV("benchmark-main-" + comm.GetInterfaceName() + f_nameSuffix + "-variable-update.csv");

    t0DataProcess.SendShutdownCommand();
}

static void Test(CommInterface& comm)
{
    uint8_t buf[0x1500];
    
    bool t1Received = false;
    bool t2Received = false;
    uint32_t t1PacketId = 0;
    uint32_t t2PacketId = 0;
    
    while(f_running) {
        comm.ReceiveT1OrT2(buf, sizeof(buf), [&](Target t, const uint8_t* buf, size_t size) {
            auto receiveTime = global_timer::now();
            if (t == Target::T1) {
                auto packet = reinterpret_cast<const SharedState_T1DataPacket*>(buf);
                t1Stats.Add(packet->stats);
                
                auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
                t1Stats.AddReceivePacketLatency(
                            receiveTime - sendTime);
                t1Received = true;
            }
            else if (t == Target::T2) {
                auto packet = reinterpret_cast<const SharedState_T2DataPacket*>(buf);
                t2Stats.Add(packet->stats);
                
                auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
                t2Stats.AddReceivePacketLatency(
                            receiveTime - sendTime);
                t2Received = true;
            }
        });
        
        if (t1Received) {
            SharedState_T1CommandPacket cmd;
            cmd.timestamp = global_timer::now().time_since_epoch().count();
            cmd.flags = 0;
            cmd.packetId = t1PacketId++;
            comm.Send(Target::T1, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
            t1Received = false;
        }
        else if (t2Received) {
            SharedState_T2CommandPacket cmd;
            cmd.timestamp = global_timer::now().time_since_epoch().count();
            cmd.flags = 0;
            cmd.packetId = t2PacketId++;
            comm.Send(Target::T2, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
            t2Received = false;
        }
    }
}

static void TestShm(CommInterface& comm)
{
    uint8_t buf[0x1500];
    
    bool t1Received = false;
    bool t2Received = false;
    uint32_t t1PacketId = 0;
    uint32_t t2PacketId = 0;
    
    while(f_running) {
        comm.ReceiveAny(buf, sizeof(buf), [&](Target t, const uint8_t* buf, size_t size) {
            auto receiveTime = global_timer::now();
            if (t == Target::T0) {
                auto packet = reinterpret_cast<const SharedState_T0ShmDataPacket*>(buf);
                t0Stats.Add(packet->stats);
                
                auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
                t0Stats.AddReceivePacketLatency(
                            receiveTime - sendTime);
            }
            else if (t == Target::T1) {
                auto packet = reinterpret_cast<const SharedState_T1DataPacket*>(buf);
                t1Stats.Add(packet->stats);

                auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
                t1Stats.AddReceivePacketLatency(
                            receiveTime - sendTime);
                t1Received = true;
            }
            else if (t == Target::T2) {
                auto packet = reinterpret_cast<const SharedState_T2DataPacket*>(buf);
                t2Stats.Add(packet->stats);

                auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
                t2Stats.AddReceivePacketLatency(
                            receiveTime - sendTime);
                t2Received = true;
            }
        });
        
        if (t1Received) {
            SharedState_T1CommandPacket cmd;
            cmd.timestamp = global_timer::now().time_since_epoch().count();
            cmd.flags = 0;
            cmd.packetId = t1PacketId++;
            comm.Send(Target::T1, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
            t1Received = false;
        }
        else if (t2Received) {
            SharedState_T2CommandPacket cmd;
            cmd.timestamp = global_timer::now().time_since_epoch().count();
            cmd.flags = 0;
            cmd.packetId = t2PacketId++;
            comm.Send(Target::T2, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
            t2Received = false;
        }
    }
}
