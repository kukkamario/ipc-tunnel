#ifndef OPENAMP_HPP_
#define OPENAMP_HPP_



#include "comm.hpp"

class OpenAMPComm final : public CommInterface {
public:
    ~OpenAMPComm();
	std::string GetInterfaceName() override;
    bool Initialize() override;
    void Send(Target t, const uint8_t* data, size_t size) override;
    Target ReceiveAnyBlock(uint8_t* data, size_t& size) override;
	uint16_t GetMaxPacketSize() const override;
private:
    int charfd = -1;
    int fd = -1;
};

#endif  // OPENAMP_HPP_
