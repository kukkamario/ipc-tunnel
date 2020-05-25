#ifndef OPENAMP_HPP_
#define OPENAMP_HPP_



#include "comm.hpp"

class OpenAMPComm final : public CommInterface {
public:
    ~OpenAMPComm();
	std::string GetInterfaceName() override;
    bool Initialize(bool blockT0) override;
    bool Send(Target t, const uint8_t* data, size_t size) override;
    size_t ReceiveT0(uint8_t* data, size_t bufSize) override;
    
    void ReceiveAny(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb) override;
    void ReceiveT1OrT2(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb) override;
    
	uint16_t GetMaxPacketSize(Target t) const override;
    
    uint8_t* MapT0SharedMemory() override;
private:
    bool InitDev(int i, bool block);
    
    int charfds[3] = {-1, -1, -1};
    int fds[3] = {-1, -1, -1};
};

#endif  // OPENAMP_HPP_
