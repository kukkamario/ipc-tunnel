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


template<typename IT>
static int64_t latencyVarianceNs(IT begin, IT end, decltype(*begin) avg) {
    return std::accumulate(begin, end,
                    int64_t(0),
                    [&](int64_t sum, global_timer::duration b){
        int64_t diff = std::chrono::duration_cast<std::chrono::nanoseconds>(b - avg).count();
        return sum + diff * diff;
    }) / (end - begin);
}

static void DoTest(CommInterface& comm)
{
    constexpr unsigned ITERATION_COUNT = 500;
    
    uint8_t packetBuffer alignas(8) [2048];
    uint16_t testedPacketSizes[] = {32, 64, 128, 256, 496 , 512, 1024, 2048};
    
    std::array<std::chrono::nanoseconds, 8> l2bLatencies;
    std::array<int64_t, 8> l2bLatencyVars;
    std::array<std::chrono::nanoseconds, 8> b2lLatencies;
    std::array<int64_t, 8> b2lLatencyVars;
    std::array<std::chrono::nanoseconds, 8> readDurations;
    std::array<int64_t, 8> readDurationVars;

    size_t packetSizeIdx = 0;
    
    for (uint16_t packetSize : testedPacketSizes) {
        if (packetSize > comm.GetMaxPacketSize(Target::T0)) break;
        
        std::cout << "Testing packet size " << packetSize << " bytes" << std::endl;
        
        if (packetSizeIdx > 0) {
            LinuxToBaremetal req;
            req.control_flags = CONTROL_FLAG_NEXT;
            req.send_timestamp = global_timer::now().time_since_epoch().count();
            while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req), sizeof(LinuxToBaremetal))) {}
            
            std::this_thread::sleep_for(std::chrono::microseconds(5000));
        }

        std::array<global_timer::duration, ITERATION_COUNT> linuxToBaremetalLatencies;
        std::array<global_timer::duration, ITERATION_COUNT> baremetalToLinuxLatencies;
    
        std::array<global_timer::duration, ITERATION_COUNT> linuxReadDurations;
        
        
        LinuxToBaremetal* req = reinterpret_cast<LinuxToBaremetal*>(packetBuffer);
        for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
            req->control_flags = 0;
            do {
                req->send_timestamp = global_timer::now().time_since_epoch().count();
            } while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(req), packetSize));

            BaremetalToLinux* resp = reinterpret_cast<BaremetalToLinux*>(packetBuffer);
            size_t respSize = comm.ReceiveT0(reinterpret_cast<uint8_t*>(resp), packetSize);
            auto receive_time = global_timer::now();

            linuxToBaremetalLatencies[i] = global_timer::duration(resp->linux_to_baremetal_latency);
            baremetalToLinuxLatencies[i] = receive_time
                    - global_timer::time_point(global_timer::duration(resp->send_timestamp));

            std::this_thread::sleep_for(std::chrono::microseconds(5000));
        }
    
        LinuxToBaremetal req_next;
        req_next.control_flags = CONTROL_FLAG_NEXT;
        req_next.send_timestamp = global_timer::now().time_since_epoch().count();
        while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req_next), sizeof(LinuxToBaremetal))) {}
        
        for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
            req->control_flags = 0;
            do {
                req->send_timestamp = global_timer::now().time_since_epoch().count();
            } while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(req), packetSize));
            std::this_thread::sleep_for(std::chrono::microseconds(500));

            auto receive_start = global_timer::now();
            BaremetalToLinux* resp = reinterpret_cast<BaremetalToLinux*>(packetBuffer);
            size_t respSize = comm.ReceiveT0(reinterpret_cast<uint8_t*>(resp), packetSize);
            auto receive_end = global_timer::now();

            linuxReadDurations[i] = receive_end - receive_start;
        }

        global_timer::duration linuxToBaremetalLatency =
                std::accumulate(linuxToBaremetalLatencies.begin(), linuxToBaremetalLatencies.end(), global_timer::duration()) / ITERATION_COUNT;
        global_timer::duration baremetalToLinuxLatency  =
                std::accumulate(baremetalToLinuxLatencies.begin(), baremetalToLinuxLatencies.end(), global_timer::duration()) / ITERATION_COUNT;
        
        global_timer::duration linuxReadDuration =
                std::accumulate(linuxReadDurations.begin(), linuxReadDurations.end(), global_timer::duration()) / ITERATION_COUNT;
    
        int64_t linuxToBaremetalLatencyVarianceNs =
                latencyVarianceNs(linuxToBaremetalLatencies.begin(), linuxToBaremetalLatencies.end(), linuxToBaremetalLatency);
        int64_t baremetalToLinuxLatencyVariance =
                latencyVarianceNs(linuxToBaremetalLatencies.begin(), linuxToBaremetalLatencies.end(), baremetalToLinuxLatency);
        
        int64_t linuxReadDurationVarianceNs =
                latencyVarianceNs(linuxReadDurations.begin(), linuxReadDurations.end(), linuxReadDuration);
        
        l2bLatencies[packetSizeIdx] = std::chrono::duration_cast<std::chrono::nanoseconds>(linuxToBaremetalLatency);
        b2lLatencies[packetSizeIdx] = std::chrono::duration_cast<std::chrono::nanoseconds>(baremetalToLinuxLatency);
        readDurations[packetSizeIdx] = std::chrono::duration_cast<std::chrono::nanoseconds>(linuxReadDuration);
        
        l2bLatencyVars[packetSizeIdx] = linuxToBaremetalLatencyVarianceNs;
        b2lLatencyVars[packetSizeIdx] = baremetalToLinuxLatencyVariance;
        readDurationVars[packetSizeIdx] = linuxReadDurationVarianceNs;
        
        ++packetSizeIdx;
    }
    LinuxToBaremetal req;
    req.control_flags = CONTROL_FLAG_SHUTDOWN;
    req.send_timestamp = global_timer::now().time_since_epoch().count();
    while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req), sizeof(LinuxToBaremetal))) {}

    std::ofstream out("latency-" + comm.GetInterfaceName() + ".csv");
    out << "packet_size\tl2b_latency\tl2b_latency_var\tb2l_latency\tb2l_latency_var\tl_read_dur\tl_read_dur_var\n";
    
    for (size_t i = 0; i < packetSizeIdx; ++i) {
        out << testedPacketSizes[i] << "\t";
        out << l2bLatencies[i].count() << "\t" << l2bLatencyVars[i] << "\t";
        out <<  b2lLatencies[i].count() << "\t" << b2lLatencyVars[i] << "\t";
        out << readDurations[i].count() << "\t" << readDurationVars[i] << "\n";
    }
}
