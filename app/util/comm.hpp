#ifndef DIPPA_COMM_HPP
#define DIPPA_COMM_HPP

#include <string>

enum class Target {
    T0,
    T1,
    T2
};

class CommInterface {
public:
    virtual ~CommInterface() {}
    virtual bool Initialize() = 0;
    virtual void Send(Target t, const uint8_t* data, size_t size) = 0;
    virtual Target ReceiveAnyBlock(uint8_t* data, size_t& size) = 0;
};


#endif  // DIPPA_COMM_HPP

