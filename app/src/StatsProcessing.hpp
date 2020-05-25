#ifndef STATS_PROCESSING_HPP_
#define STATS_PROCESSING_HPP_
#include "shared_state.h"
#include "globaltimer.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <numeric>


class StatsProcessing {
public:
	StatsProcessing(size_t reserve = 0) {
		if (reserve > 0) {
			iterationNumbers.reserve(reserve);
			timeLevelDurations.reserve(reserve);
			timeLevelStartTimes.reserve(reserve);
			sentPacketLatencies.reserve(reserve);
			sendTimes.reserve(reserve);
			receivePacketLatencies.reserve(reserve);
		}
	}
	
	void Add(const SharedState_TimeLevelStats& stats);
	void AddReceivePacketLatency(global_timer::duration dur);
	
	void WriteCSV(const std::string& fileName);
private:
	std::vector<uint32_t> iterationNumbers;
	std::vector<global_timer::time_point> timeLevelStartTimes;
	std::vector<uint32_t> timeLevelDurations;
	std::vector<uint32_t> sentPacketLatencies;
	std::vector<uint32_t> sendTimes;
	std::vector<global_timer::duration> receivePacketLatencies;
	
	int64_t iterationNumber = -1;
	uint32_t totalDroppedPackets = 0;
	uint32_t totalMissedStats = 0;
	uint32_t prevStartTimeLow = 0;
	uint32_t startTimeHigh = 0;
	uint32_t lastCommandPacketId = 0xFFFFFFFF;
};


void StatsProcessing::Add(const SharedState_TimeLevelStats &stats)
{
	if (stats.iterationNumber > iterationNumber) {
		uint32_t itCount = stats.iterationNumber - iterationNumber;
		if (itCount > SHAREDSTATE_BACKLOG) {
			totalMissedStats += itCount - SHAREDSTATE_BACKLOG;
			itCount = SHAREDSTATE_BACKLOG;
		}
		
		for (int i = itCount - 1; i >= 0; --i) {
			uint32_t startTimeLow = stats.timeLevelStartTimes[i];
			if (startTimeLow < prevStartTimeLow) {
				startTimeHigh += 1;
			}
			prevStartTimeLow = startTimeLow;
			
			iterationNumbers.push_back(stats.iterationNumber - i);
			timeLevelStartTimes.push_back(global_timer::time_point(global_timer::duration(((uint64_t)startTimeHigh << 32u) + startTimeLow)));
			timeLevelDurations.push_back(stats.timeLevelDurations[i]);
		}
		
		if (lastCommandPacketId != stats.commandPacketId) {
			lastCommandPacketId = stats.commandPacketId;
			sentPacketLatencies.push_back(stats.commandPacketLatency);
		}
		
		iterationNumber = stats.iterationNumber;
		totalDroppedPackets = stats.totalDroppedPackets;
		
		sendTimes.push_back(stats.lastPacketSendTime);
	}
}

void StatsProcessing::AddReceivePacketLatency(global_timer::duration dur)
{
	receivePacketLatencies.push_back(dur);
}

void StatsProcessing::WriteCSV(const std::string &fileName)
{
	std::ofstream out(fileName);
	
	global_timer::duration avgSendTime = std::accumulate(
	            sendTimes.begin(),
	            sendTimes.end(),
	            global_timer::duration(0),
	            [&](global_timer::duration sum, uint32_t dur) {
	        return sum + global_timer::duration(dur);
	}) / sendTimes.size();
	
	uint32_t sendTimeVarianceNs = std::accumulate(sendTimes.begin(), sendTimes.end(),
	                int64_t(0),
	                [&](int64_t sum, uint32_t dur){
	        
        int64_t diff = std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(dur) - avgSendTime).count();
        return sum + diff * diff;
    }) / sendTimes.size();
	
	global_timer::duration recordDuration = timeLevelStartTimes.back() - timeLevelStartTimes.front();
	
	auto expectedStartTime = [&](uint32_t iteration) {
		return global_timer::time_point(recordDuration * iteration / iterationNumbers.back());
	};
	
	out << "send_time_avg_ns\t" << std::chrono::duration_cast<std::chrono::nanoseconds>(avgSendTime).count() << '\n';
	out << "send_time_variance_ns\t" << sendTimeVarianceNs << '\n';
	out << "dropped_packets\t" << totalDroppedPackets << '\n';
	out << "missed_stats\t" << totalMissedStats << '\n';
	
	out << "\niteration\tstart_time(ns)\texpected_start_time(ns)\tstart_time_expectation_offset(ns)\tduration(ns)\n";
	
	for (size_t i = 0; i < iterationNumbers.size(); ++i) {
		auto startTimeNs= std::chrono::duration_cast<std::chrono::nanoseconds>(timeLevelStartTimes[i] - timeLevelStartTimes.front());
		auto expectedStartTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(expectedStartTime(iterationNumbers[i]).time_since_epoch());
		out << iterationNumbers[i] << '\t'
		    << startTimeNs.count() << '\t'
		    << expectedStartTimeNs.count() << '\t'
		    << (startTimeNs - expectedStartTimeNs).count() << '\t'
		    << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(timeLevelDurations[i])).count()
		    << '\n';
	}
	
	
	out << "\n\n\n";
	out << "send_latencies(ns)\treceive_latencies(ns)\n";
	
	size_t latencyCountMax = std::max(receivePacketLatencies.size(), sentPacketLatencies.size());
	
	for (size_t i = 0; i < latencyCountMax; ++i) {
		if (i < receivePacketLatencies.size()) {
			out << std::chrono::duration_cast<std::chrono::nanoseconds>(receivePacketLatencies[i]).count();
		}
		out << '\t';
		if (i < sentPacketLatencies.size()) {
			out << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(sentPacketLatencies[i])).count();
		}
		out << '\n';
	}
}


#endif  // STATS_PROCESSING_HPP_
