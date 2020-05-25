#ifndef UTIL_IPC_TUNNEL_HPP_
#define UTIL_IPC_TUNNEL_HPP_

#include "comm.hpp"

class IpcTunnel final : public CommInterface {
public:
	enum class Memory {
		DDR,
		OCM
	};

	IpcTunnel(Memory mem);
	~IpcTunnel();
	
    std::string GetInterfaceName() override;
    bool Initialize(bool blockT0) override;
    bool Send(Target t, const uint8_t* data, size_t size) override;
    size_t ReceiveT0(uint8_t* data, size_t bufSize) override;

    void ReceiveAny(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb) override;
    void ReceiveT1OrT2(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb) override;
    
    uint16_t GetMaxPacketSize(Target t) const override;
    
    virtual uint8_t* MapT0SharedMemory() override;
private:
	Memory mem;
	
	int fds[3];
    int nfds;
    
    uint8_t* shm = 0;
};


#endif  // UTIL_IPC_TUNNEL_HPP_
