#ifndef OPENAMP_HPP_
#define OPENAMP_HPP_



#include "comm.hpp"

class OpenAMPComm final : public CommInterface {
public:
    ~OpenAMPComm();
    bool Initialize() override;
    void Send(Target t, const uint8_t* data, size_t size) override;
    Target ReceiveAnyBlock(uint8_t* data, size_t& size) override;

private:
    int charfd = -1;
    int fd = -1;
};

#endif  // OPENAMP_HPP_
