#include "ipc_tunnel.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

IpcTunnel::IpcTunnel(IpcTunnel::Memory mem) :
    mem(mem)
{
    
}

IpcTunnel::~IpcTunnel()
{
    for (int i = 0; i < 3; ++i) {
        if (fds[i] > 0) {
            close(fds[i]);
        }
    }
}

std::string IpcTunnel::GetInterfaceName()
{
    return std::string("IpcTunnel") + (mem == Memory::OCM ? "OCM" : "DDR");
}

bool IpcTunnel::Initialize(bool blockT0)
{
    int devIndex = mem == Memory::OCM ? 0 : 3;

    for (int i = 0; i < 3; ++i) {
        std::string devName("/dev/ipc_tunnel");
        devName += std::to_string(devIndex);
        fds[i] = open(devName.c_str(), O_RDWR | ((blockT0 && i == 0) ? 0 : O_NONBLOCK) );
        devIndex += 1;
    }
    
    return fds[0] > 0 && fds[1] > 0 && fds[2] > 0;
}

bool IpcTunnel::Send(Target t, const uint8_t *data, size_t size)
{
    return write(fds[(int)t], data, size) > 0;
}

size_t IpcTunnel::ReceiveT0(uint8_t* data, size_t bufSize)
{
    ssize_t readBytes = read(fds[0], data, bufSize);
    if (readBytes > 0) {
        return readBytes;
    }
    
    return 0;
}

void IpcTunnel::ReceiveT1OrT2(uint8_t* buf, size_t size, const std::function<void(Target, const uint8_t*, size_t)>& receiveCb)
{
    fd_set fd_set_t1t2;
    FD_ZERO(&fd_set_t1t2);
    FD_SET(fds[1], &fd_set_t1t2);
    FD_SET(fds[2], &fd_set_t1t2);
    
    int readyFds = select(2, &fd_set_t1t2, 0, 0, 0);
    if (readyFds > 0) {
        if (FD_ISSET(fds[1], &fd_set_t1t2)) {
            ssize_t readBytes = read(fds[1], buf, size);
            if (readBytes > 0) {
                receiveCb(Target::T1, buf, readBytes);
            }
        }
        if (FD_ISSET(fds[2], &fd_set_t1t2)) {
            ssize_t readBytes = read(fds[2], buf, size);
            if (readBytes > 0) {
                receiveCb(Target::T2, buf, readBytes);
            }
        }
    }
}

uint16_t IpcTunnel::GetMaxPacketSize(Target t) const
{
    static constexpr uint16_t sizes[3] = { 0x780, 0x1200, 0x780 };
    
    return sizes[(int)t];
}
