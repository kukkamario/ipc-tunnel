#include "globaltimer.hpp"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <cassert>

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

#define GLOBAL_TIMER_PHYS_ADDR 0xF8F00200U
namespace {

static volatile uint32_t* globalTimerRegs = 0;

class InitGlobalTimerHelper {
public:
    InitGlobalTimerHelper() {
        memfd = open("/dev/mem", O_RDWR | O_SYNC);
        assert(memfd >= 0);

        mappedAddress = mmap(0, MAP_SIZE, PROT_READ, MAP_SHARED, memfd, GLOBAL_TIMER_PHYS_ADDR & ~MAP_MASK);

        globalTimerRegs = (uint32_t*)((uint8_t*)mappedAddress + (GLOBAL_TIMER_PHYS_ADDR & MAP_MASK));
    }

    ~InitGlobalTimerHelper() {
        munmap(mappedAddress, MAP_SIZE);
        close(memfd);
    }

    int memfd  = -1;
    void* mappedAddress = 0;
} s_init;

}


global_timer::time_point global_timer::now()
{
    uint32_t high, low;
    do
    {
        high = globalTimerRegs[1];
        low = globalTimerRegs[0];
    } while(globalTimerRegs[1] != high);  // Handle lower reg overflow

    uint64_t ticks = ((uint64_t)high << 32) | low;
    return time_point(duration(ticks));
}
