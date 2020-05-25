#include "comm.hpp"
#include "openamp.hpp"
#include "globaltimer.hpp"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>

#include "packets.h"
#include <thread>
#include <fstream>
#include <iomanip>

static void DoTest(CommInterface& comm);

int main(int argc, char *argv[])
{
    auto comm = CreateFromArgs(argc, argv);
    if (!comm) {
        return 1;
    }
    
    if (!comm->Initialize(true)) {
        std::cerr << "Failed to initialize communication" << std::endl;
        return 1;
    }

    DoTest(*comm);

    return 0;
}
static constexpr unsigned ITERATION_COUNT = 10000;
static constexpr unsigned REPEAT_COUNT = 10;

static uint16_t f_testedPacketSizes[] = {32, 64, 128, 256, 496 , 512, 1024, 2048};
static std::array<std::array<global_timer::duration, ITERATION_COUNT>, REPEAT_COUNT> linuxToBaremetalLatencies;
static std::array<std::array<global_timer::duration, ITERATION_COUNT>, REPEAT_COUNT> baremetalToLinuxLatencies;


template<typename IT>
static int64_t latencyVarianceNs(IT begin, IT end, decltype(*begin) avg) {
    return std::accumulate(begin, end,
                    int64_t(0),
                    [&](int64_t sum, global_timer::duration b){
        int64_t diff = std::chrono::duration_cast<std::chrono::nanoseconds>(b - avg).count();
        return sum + diff * diff;
    }) / (end - begin);
}

static uint8_t f_buffer[1024 * 16] __attribute__ ((aligned (8)));

static void DoTest(CommInterface& comm)
{
    std::ofstream out_f("throughput-" + comm.GetInterfaceName() + ".csv");

    
    for (uint16_t packetSize : f_testedPacketSizes) {
        std::array<global_timer::duration, REPEAT_COUNT> baremetalToLinuxReceiveTime;
        std::array<global_timer::duration, REPEAT_COUNT> linuxToBaremetalReceiveTime;
        
        std::array<double, REPEAT_COUNT> b2lPacketThroughput;
        std::array<double, REPEAT_COUNT> l2bPacketThroughput;
        
        if (packetSize > comm.GetMaxPacketSize(Target::T0)) break;
        
        std::cout << "Testing throughput with packet size " << packetSize << std::endl;
        
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            //std::cout << "App phase=0" << std::endl;
            {  // phase = 0
                LinuxToBaremetal* req = reinterpret_cast<LinuxToBaremetal*>(f_buffer);
                for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
                    req->control_flags = (i + 1 == ITERATION_COUNT) ? CONTROL_FLAG_NEXT : 0;
                    req->send_timestamp = global_timer::now().time_since_epoch().count();
                    req->packet_id = i;
                    
                    while (!comm.Send(Target::T0, f_buffer, packetSize)) {}
                }
            }
            //std::cout << "App phase=1" << std::endl;
            {  // phase = 1
                global_timer::time_point firstPacketReceiveTime{};
                global_timer::time_point receive_time;
                for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
                    size_t respSize = comm.ReceiveT0(f_buffer, sizeof(f_buffer));
                    receive_time = global_timer::now();
                    
                    if (i == 0) firstPacketReceiveTime = receive_time;
                    
                    BaremetalToLinux* resp = reinterpret_cast<BaremetalToLinux*>(f_buffer);
                    linuxToBaremetalLatencies[r][i] = global_timer::duration(resp->linux_to_baremetal_latency);
                    baremetalToLinuxLatencies[r][i] = receive_time
                            - global_timer::time_point(global_timer::duration(resp->send_timestamp));
                }
                baremetalToLinuxReceiveTime[r] = receive_time - firstPacketReceiveTime;
            }
            BaremetalToLinux* resp = reinterpret_cast<BaremetalToLinux*>(f_buffer);
            //std::cout << "App phase=2. End pid=" << resp->packet_id << std::endl;
            {  // phase = 2
                LinuxToBaremetal req;
                req.control_flags = CONTROL_FLAG_NEXT;
                req.send_timestamp = global_timer::now().time_since_epoch().count();
                req.packet_id = 0xFFFFFFFF;
                while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req), sizeof(LinuxToBaremetal))) {}
                
                size_t respSize = comm.ReceiveT0(f_buffer, sizeof(f_buffer));
                BaremetalToLinux* resp = reinterpret_cast<BaremetalToLinux*>(f_buffer);
                //std::cout << "App phase to 0: pid=" << resp->packet_id << std::endl;
                
                linuxToBaremetalReceiveTime[r] = global_timer::duration(resp->linux_to_baremetal_latency);
                
                b2lPacketThroughput[r] = (ITERATION_COUNT - 1) / std::chrono::duration<double>(baremetalToLinuxReceiveTime[r]).count();
                l2bPacketThroughput[r] = (ITERATION_COUNT - 1) / std::chrono::duration<double>(linuxToBaremetalReceiveTime[r]).count();
            }
        }

        out_f << std::setprecision(20);
        
        out_f << "packet_size\t" << packetSize << "\n";
        out_f << "l_to_b_packet_throughput(packets/s)";
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            out_f << "\t" << l2bPacketThroughput[r];
        }
        out_f << '\n';
        
        out_f << "b_to_l_packet_throughput(packets/s)";
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            out_f << "\t" << b2lPacketThroughput[r];
        }
        out_f << '\n';
        
        out_f << "l_to_b_byte_throughput(B/s)";
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            out_f << "\t" << l2bPacketThroughput[r] * packetSize;
        }
        out_f << '\n';
        
        out_f << "b_to_l_byte_throughput(B/s)";
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            out_f << "\t" << b2lPacketThroughput[r] * packetSize;
        }
        out_f << '\n';
        
        out_f << "latencies_b_to_l\n";
        
        out_f << "iteration";
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            out_f << "\t" << "latency_b_to_l[" << r << "](ns)";
        }
        out_f << '\n';
        
        for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
            out_f << i;
            for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
                out_f << "\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(baremetalToLinuxLatencies[r][i]).count();
            }
            out_f << "\n";
        }
        
        out_f << '\n';
        
        out_f << "latencies_l_to_b\n";
        
        out_f << "iteration";
        for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
            out_f << "\t" << "latency_l_to_b[" << r << "](ns)";
        }
        out_f << '\n';
        
        for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
            out_f << i;
            for (unsigned r = 0; r < REPEAT_COUNT; ++r) {
                out_f << "\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(linuxToBaremetalLatencies[r][i]).count();
            }
            out_f << "\n";
        }
        
        out_f << '\n';
    }
}
