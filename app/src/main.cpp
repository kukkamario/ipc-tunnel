#include "comm.hpp"
#include "openamp.hpp"
#include "globaltimer.hpp"
#include "shared_state.h"
#include <iostream>
#include <thread>
#include "StatsProcessing.hpp"
#include "t0dataprocess.hpp"
#include <atomic>

static void Test(CommInterface& comm);

static StatsProcessing t0Stats, t1Stats, t2Stats;

static std::atomic_bool f_running{true};

static void T0Thread(CommInterface& comm);

#define ITERATION_LIMIT (20000 * 60)  // ~60s

int main(int argc, char *argv[])
{
    auto comm = CreateFromArgs(argc, argv);
    if (!comm) {
        return 1;
    }
    
    comm->Initialize(true);
    
    std::thread t0Thread([&](){
        T0Thread(*comm);
    });
    
    Test(*comm);
    t0Thread.join();
    
    t0Stats.WriteCSV("benchmark-main-" + comm->GetInterfaceName() + "-t0.csv");
    t1Stats.WriteCSV("benchmark-main-" + comm->GetInterfaceName() + "-t1.csv");
    t2Stats.WriteCSV("benchmark-main-" + comm->GetInterfaceName() + "-t2.csv");
    return 0;
}

static void T0Thread(CommInterface& comm) {
    T0DataProcess t0DataProcess(comm);
    uint8_t buf[0x1500];
    
    // Send start packet so the actual scheduling starts.
    // This is to avoid baremetal side filling ring buffers and dropping packets before
    // Linux side starts
    comm.Send(Target::T0, buf, 1);
    
    for (int i = 0; i < ITERATION_LIMIT; ++i) {
        size_t receivedBytes = comm.ReceiveT0(buf, sizeof(buf));
        auto receiveTime = global_timer::now();
        
        const SharedState_T0DataPacket* packet = reinterpret_cast<const SharedState_T0DataPacket*>(buf);
        auto sendTime = global_timer::time_point(global_timer::duration(packet->timestamp));
        t0DataProcess.HandleNewVariableData(packet->variables);
        t0Stats.AddReceivePacketLatency(receiveTime - sendTime);
        t0Stats.Add(packet->stats);
        
        if (i & 1) t0DataProcess.SendRandomVariableUpdate();
    }
    
    t0DataProcess.WriteCSV("benchmark-main-" + comm.GetInterfaceName() + "-variable-update.csv");
    f_running = false;
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
        if (t2Received) {
            SharedState_T2CommandPacket cmd;
            cmd.timestamp = global_timer::now().time_since_epoch().count();
            cmd.flags = 0;
            cmd.packetId = t2PacketId++;
            comm.Send(Target::T2, reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd));
            t2Received = false;
        }
    }
}
