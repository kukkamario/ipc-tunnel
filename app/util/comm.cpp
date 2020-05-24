#include "comm.hpp"
#include "ipc_tunnel.hpp"
#include "openamp.hpp"
#include <cstring>
#include <iostream>

std::unique_ptr<CommInterface> CreateFromArgs(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Expecting \"amp\", \"ipc_ocm\" or \"ipc_ddr\" as a parameter" << std::endl;
		return nullptr;
	}
	
	if (std::strcmp(argv[1], "amp") == 0) {
		return std::unique_ptr<CommInterface>(new OpenAMPComm());
	}
	if (std::strcmp(argv[1], "ipc_ocm") == 0) {
		return std::unique_ptr<CommInterface>(new IpcTunnel(IpcTunnel::Memory::OCM));
	}
	if (std::strcmp(argv[1], "ipc_ddr") == 0) {
		return std::unique_ptr<CommInterface>(new IpcTunnel(IpcTunnel::Memory::DDR));
	}
	
	std::cerr << "Expecting \"amp\", \"ipc_ocm\" or \"ipc_ddr\" as a parameter" << std::endl;
	return nullptr;
}
