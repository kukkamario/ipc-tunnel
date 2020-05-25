#ifndef DIPPA_COMM_HPP
#define DIPPA_COMM_HPP

#include <string>
#include <memory>
#include <functional>

enum class Target {
    T0 = 0,
    T1 = 1,
    T2 = 2
};

class CommInterface {
public:
    virtual ~CommInterface() {}
    virtual std::string GetInterfaceName() = 0;
    virtual bool Initialize(bool blockT0) = 0;
    virtual bool Send(Target t, const uint8_t* data, size_t size) = 0;
    

    virtual size_t ReceiveT0(uint8_t* data, size_t bufSize) = 0;

    virtual void ReceiveAny(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb) = 0;
    virtual void ReceiveT1OrT2(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb) = 0;
    
    virtual uint16_t GetMaxPacketSize(Target t) const = 0;
    
    virtual uint8_t* MapT0SharedMemory() = 0;
};

std::unique_ptr<CommInterface> CreateFromArgs(int argc, char *argv[]);


#endif  // DIPPA_COMM_HPP

