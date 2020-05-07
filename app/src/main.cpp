#include "comm.hpp"
#include "openamp.hpp"
#include "globaltimer.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    auto start = global_timer::now();
    OpenAMPComm comm;
    comm.Initialize();

    auto end = global_timer::now();

    auto diff = end - start;
    std::cout << "OpenAMP init took: " << diff.count() << " ticks, "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() << " ns" << std::endl;

    return 0;
}
