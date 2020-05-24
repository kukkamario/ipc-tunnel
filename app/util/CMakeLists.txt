add_library(util
    openamp.cpp
	comm.cpp
	globaltimer.cpp
	ipc_tunnel.cpp)
target_include_directories(util INTERFACE .)
