#include "shared_state.h"
#include "comm.hpp"
#include "globaltimer.hpp"
#include <array>
#include <cstring>
#include <fstream>

class T0DataProcess {
public:
	T0DataProcess(CommInterface& comm) : comm(comm) {}
	
	void HandleNewVariableData(const SharedState_Variables& vars);
	
	void SendRandomVariableUpdate();
	
	void WriteCSV(const std::string& fileName);
private:
	uint32_t RandomNum() {
		randomSeed = (214013*randomSeed+2531011);
		return (randomSeed>>16) & 0x7FFF;
	}
	
	CommInterface& comm;
	
	// Delay between sending variable update command and baremetal side handling that
	std::vector<uint32_t> varUpdateDelaysBuf;
	
	// Delay between sending variable update command and seeing results it causes
	std::vector<uint32_t> varUpdateSeenDelayBuf;
	
	
	uint32_t handledPacketId = 0xFFFFFFFF;
	
	uint32_t packetIdCounter = 0;
	
	uint32_t randomSeed = 4512431;
	
	alignas (8) std::array<uint8_t, sizeof(SharedState_T0CommandPacket) + 32> sendPacketBuffer;
	uint32_t sendPacketSize = 0;
	uint32_t delayedPacketCounter = 0;
};

void T0DataProcess::HandleNewVariableData(const SharedState_Variables &vars)
{
	auto now = global_timer::now();
	if (vars.lastSetPacketId != handledPacketId) {
		varUpdateDelaysBuf.push_back(vars.lastSetReceiveTimestamp - vars.lastSetSendTimestamp);
		varUpdateSeenDelayBuf.push_back((now - global_timer::time_point(global_timer::duration(vars.lastSetSendTimestamp))).count());
		handledPacketId = vars.lastSetPacketId;
	}
}

void T0DataProcess::SendRandomVariableUpdate()
{
	constexpr int setVariableCount = 3;
	
	// Generate only new packet if previous 
	if (sendPacketSize == 0) {
		uint32_t rand = RandomNum();
		
		SharedState_T0CommandPacket* packet = reinterpret_cast<SharedState_T0CommandPacket*>(
		            sendPacketBuffer.data());
		
		packet->flags = 0;
		packet->packetId = packetIdCounter++;
		packet->timestamp = global_timer::now().time_since_epoch().count();
		packet->varSetCommands = 2;
		
		uint8_t* varSetPtr = sendPacketBuffer.data() + sizeof(SharedState_T0CommandPacket);
		for (int i = 0; i < setVariableCount; ++i) {
			SharedState_VariableSet* varSet = reinterpret_cast<SharedState_VariableSet*>(varSetPtr);
			varSetPtr += sizeof(SharedState_VariableSet);
			varSet->variableType = rand & 0x3;
			varSet->variableId = (rand + 13) % SHAREDSTATE_VARIABLE_COUNT;
			switch(varSet->variableType) {
			case SHAREDSTATE_VAR_DOUBLE: {
				double val = rand * 12.04 + 23.2;
				std::memcpy(varSetPtr, &val, sizeof(double));
				varSetPtr += sizeof(double);
				break;
			}
			case SHAREDSTATE_VAR_FLOAT: {
				float val = rand * 3.0f - 0.23f;
				memcpy(varSetPtr, &val, sizeof(float));
				varSetPtr += sizeof(float);
				break;
			}
			case SHAREDSTATE_VAR_SHORT: {
				uint16_t val = rand;
				memcpy(varSetPtr, &val, sizeof(uint16_t));
				varSetPtr += sizeof(uint16_t);
				break;
			}
			case SHAREDSTATE_VAR_BYTE: {
				uint8_t val = rand * 12.04 + 23.2;
				memcpy(varSetPtr, &val, sizeof(uint8_t));
				varSetPtr += sizeof(uint8_t);
				break;
			}
			}
			rand = rand >> 2;
		}
		
		sendPacketSize = varSetPtr - sendPacketBuffer.data();
		
	}

	if (!comm.Send(Target::T0, sendPacketBuffer.data(), sendPacketSize)) {
		// Can't send packet? Buffer is likely full
		++delayedPacketCounter;
	}
	else {
		sendPacketSize = 0;
	}
}

void T0DataProcess::WriteCSV(const std::string &fileName)
{
	std::fstream out(fileName);
	
	out << "delayed_packets\t" << delayedPacketCounter << "\n\n";
	out << "i\tcommand_send_delay(ns)\taction_result_delay(ns)\t\n";
	
	for (size_t i = 0; i < varUpdateDelaysBuf.size(); ++i) {
		out << i << '\t' 
		    << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(varUpdateDelaysBuf[i])).count() << '\t'
		    << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(varUpdateSeenDelayBuf[i])).count() << '\n';
	}
}
