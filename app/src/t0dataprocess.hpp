#include "shared_state.h"
#include "comm.hpp"
#include "globaltimer.hpp"
#include <array>
#include <cstring>
#include <fstream>
#include <vector>
#include <iostream>

class T0DataProcess {
public:
	T0DataProcess(CommInterface& comm) : comm(comm) {
		shm = (SharedState_T0SharedMemory*)comm.MapT0SharedMemory();
	}
	
	bool HasShm() const { return shm != nullptr; }
	
	bool UpdateVariablesFromShm();
	
	void HandleNewVariableData(const SharedState_Variables& vars);
	
	void SendRandomVariableUpdate();
	
	void WriteCSV(const std::string& fileName);
    
    void SendShutdownCommand();
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
    std::vector<uint32_t> shmUpdateTimes;
    std::vector<uint32_t> shmVarDataDelays;
    std::vector<uint32_t> shmUpdateTimesLinux;
	
	SharedState_T0SharedMemory* shm = nullptr;
	uint32_t handledPacketId = 0;
	
	uint32_t packetIdCounter = 1;
	
	uint32_t randomSeed = 4512431;
	
	alignas (8) std::array<uint8_t, sizeof(SharedState_T0CommandPacket) + 32> sendPacketBuffer;
	uint32_t sendPacketSize = 0;
	uint32_t delayedPacketCounter = 0;
    
    uint32_t shmPrevCounter = 0;
};

bool T0DataProcess::UpdateVariablesFromShm()
{
    SharedState_Variables vars;
    uint32_t prevUpdateTime;
	uint64_t timestamp;
    
	/* Block if other core just happens to be copying new values to the shared memory */
    auto updateStart = global_timer::now();
    uint32_t startVal;
    do {
        while(((startVal = __atomic_load_n(&shm->updateAtomicCounter, __ATOMIC_ACQUIRE)) & 1) == 1) { }
        
        /* No new values */
        if (startVal == shmPrevCounter) return false;
        
        timestamp = shm->timestamp;
        vars = shm->vars;
        prevUpdateTime = shm->prevUpdateTime;
        
        // Verify that synchronization variable hasn't changed during the copying
    } while ( __atomic_load_n(&shm->updateAtomicCounter, __ATOMIC_ACQUIRE) != startVal);
    
    shmPrevCounter = startVal;
    auto updateEnd = global_timer::now();
    if (prevUpdateTime > 0) shmUpdateTimes.push_back(prevUpdateTime);
    shmUpdateTimesLinux.push_back((updateEnd - updateStart).count());
    shmVarDataDelays.push_back((updateEnd - global_timer::time_point(global_timer::duration(timestamp))).count());
    HandleNewVariableData(vars);
    return true;
}

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
		packet->varSetCommands = setVariableCount;
		
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
		std::cerr << "Delayed T0 packet" << std::endl;
	}
	else {
		sendPacketSize = 0;
	}
}

void T0DataProcess::WriteCSV(const std::string &fileName)
{
	std::ofstream out(fileName);
	
    bool shmInUse = shmUpdateTimes.size() > 0;
    
	out << "delayed_packets\t" << delayedPacketCounter << "\n\n";
	out << "i\tcommand_send_delay(ns)\taction_result_delay(ns)";
    
    if (shmInUse) {
        out << "\tshm_copy_time_baremetal(ns)\tshm_block_time_linux\tvar_data_delay(ns)";
    }
    out << '\n';
    
    size_t resultMaxCount = std::max(varUpdateDelaysBuf.size(), shmVarDataDelays.size());
    
    
	for (size_t i = 0; i < resultMaxCount; ++i) {
		out << i << '\t';
        if (i < varUpdateDelaysBuf.size()) {
            out << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(varUpdateDelaysBuf[i])).count() << '\t'
                << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(varUpdateSeenDelayBuf[i])).count() << '\t';
        }
        else {
            out << "\t\t";
        }
        
        if (shmInUse) {
            if (i < shmUpdateTimes.size()) {
                out << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(shmUpdateTimes[i])).count() << '\t';
            }
            else {
                out << '\t';
            }
            
            if (i < shmUpdateTimesLinux.size()) {
                out << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(shmUpdateTimesLinux[i])).count() << '\t';
            }
            else {
                out << '\t';
            }
            
            if (i < shmVarDataDelays.size()) {
                out << std::chrono::duration_cast<std::chrono::nanoseconds>(global_timer::duration(shmVarDataDelays[i])).count() << '\t';
            }
            else {
                out << '\t';
            }
        }
        out << '\n';
    }
}

void T0DataProcess::SendShutdownCommand()
{
    SharedState_T0CommandPacket* packet = reinterpret_cast<SharedState_T0CommandPacket*>(
                sendPacketBuffer.data());
    
    packet->flags = 0xDEAD;
    packet->packetId = packetIdCounter++;
    packet->timestamp = global_timer::now().time_since_epoch().count();
    packet->varSetCommands = 0;
    
    while (!comm.Send(Target::T0, sendPacketBuffer.data(), sizeof(SharedState_T0CommandPacket))) { }
}
