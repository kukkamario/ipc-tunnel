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
    constexpr unsigned ITERATION_COUNT = 1000;

    std::array<global_timer::duration, ITERATION_COUNT> linuxToBaremetalLatencies;
    std::array<global_timer::duration, ITERATION_COUNT> baremetalToLinuxLatencies;

    std::array<global_timer::duration, ITERATION_COUNT> linuxReadDurations;
    
    for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
        LinuxToBaremetal req;
        req.control_flags = 0;
        req.send_timestamp = global_timer::now().time_since_epoch().count();
        while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req), sizeof(LinuxToBaremetal))) {
            req.send_timestamp = global_timer::now().time_since_epoch().count();
        }

        BaremetalToLinux resp;
        size_t respSize = comm.ReceiveT0(reinterpret_cast<uint8_t*>(&resp), sizeof(resp));
        auto receive_time = global_timer::now();

        linuxToBaremetalLatencies[i] = global_timer::duration(resp.linux_to_baremetal_latency);
        baremetalToLinuxLatencies[i] = receive_time
                - global_timer::time_point(global_timer::duration(resp.send_timestamp));

        std::this_thread::sleep_for(std::chrono::microseconds(5000));
    }

    LinuxToBaremetal req_next;
    req_next.control_flags = CONTROL_FLAG_NEXT;
    req_next.send_timestamp = global_timer::now().time_since_epoch().count();
    while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req_next), sizeof(LinuxToBaremetal))) {}
    
    for (unsigned i = 0; i < ITERATION_COUNT; ++i) {
        LinuxToBaremetal req;
        req.control_flags = 0;
        req.send_timestamp = global_timer::now().time_since_epoch().count();
        while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req), sizeof(LinuxToBaremetal))) {
            req.send_timestamp = global_timer::now().time_since_epoch().count();
        }

        std::this_thread::sleep_for(std::chrono::microseconds(500));

        auto receive_start = global_timer::now();
        BaremetalToLinux resp;
        size_t respSize = comm.ReceiveT0(reinterpret_cast<uint8_t*>(&resp), sizeof(resp));
        auto receive_end = global_timer::now();

        linuxReadDurations[i] = receive_end - receive_start;
    }
    
    LinuxToBaremetal req;
    req.control_flags = CONTROL_FLAG_SHUTDOWN;
    req.send_timestamp = global_timer::now().time_since_epoch().count();
    while (!comm.Send(Target::T0, reinterpret_cast<const uint8_t*>(&req), sizeof(LinuxToBaremetal))) {}
    

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
    

    std::cout << "Linux to baremetal:\n"
              << "\tAverage latency: " << std::chrono::duration_cast<std::chrono::nanoseconds>(linuxToBaremetalLatency).count() << " ns\n"
              << "\tAverage variance: " << linuxToBaremetalLatencyVarianceNs << " ns^2\n"
              << "\tAverage std-dev: " << std::sqrt(linuxToBaremetalLatencyVarianceNs) << " ns\n\n";
    std::cout << "Baremetal to linux:\n"
              << "\tAverage latency: " << std::chrono::duration_cast<std::chrono::nanoseconds>(baremetalToLinuxLatency).count() << " ns\n"
              << "\tAverage variance: " << baremetalToLinuxLatencyVariance << " ns^2\n"
              << "\tAverage std-dev: " << std::sqrt(baremetalToLinuxLatencyVariance) << " ns\n\n";
    std::cout << "Linux read duration:\n"
              << "\tAverage latency: " << std::chrono::duration_cast<std::chrono::nanoseconds>(linuxReadDuration).count() << " ns\n"
              << "\tAverage variance: " << linuxReadDurationVarianceNs << " ns^2\n"
              << "\tAverage std-dev: " << std::sqrt(linuxReadDurationVarianceNs) << " ns\n\n";
}
