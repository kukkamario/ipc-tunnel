SET(CMAKE_SYSTEM_NAME Linux)

SET(CMAKE_SYSTEM_VERSION 1)

SET(CMAKE_CROSSCOMPILING ON)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard")
SET(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -lpthread")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard")
SET(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -lpthread")

# ARM_LINUX_TOOLCHAIN_CONF_DIR should be defined in a file called TC-gcc-arm-linux-conf.cmake
# Try first from directory defined by environment variable ARM_TOOLCHAIN_CONF_DIR
# If not found
IF("$ENV{ARM_LINUX_TOOLCHAIN_CONF_DIR}" STREQUAL "")
    SET(ARM_TOOLCHAIN_CONF_DIR_LOCAL "${CMAKE_CURRENT_LIST_DIR}")
    MESSAGE(STATUS "Environmental variable ARM_LINUX_TOOLCHAIN_CONF_DIR missing, using ${CMAKE_CURRENT_LIST_DIR}")
ELSE("$ENV{ARM_LINUX_TOOLCHAIN_CONF_DIR}" STREQUAL "")
    SET(ARM_TOOLCHAIN_CONF_DIR_LOCAL "$ENV{ARM_LINUX_TOOLCHAIN_CONF_DIR}")
ENDIF("$ENV{ARM_LINUX_TOOLCHAIN_CONF_DIR}" STREQUAL "")

MESSAGE(STATUS "Using ${ARM_TOOLCHAIN_CONF_DIR_LOCAL}/TC-gcc-arm-linux-conf.cmake")
INCLUDE(${ARM_TOOLCHAIN_CONF_DIR_LOCAL}/TC-gcc-arm-linux-conf.cmake)

SET(PREFIX "arm-linux-gnueabihf")
SET(POSTFIX "")

if (WIN32)
    SET(POSTFIX ".exe")
else()
    SET(POSTFIX "")
endif()

# specify the cross compiler
set(CMAKE_C_COMPILER ${ARM_CODE_GENERATION_TOOLS_DIR}/bin/${PREFIX}-gcc${POSTFIX})
set(CMAKE_CXX_COMPILER ${ARM_CODE_GENERATION_TOOLS_DIR}/bin/${PREFIX}-g++${POSTFIX})
set(CMAKE_C_LINKER ${ARM_CODE_GENERATION_TOOLS_DIR}/bin/${PREFIX}-ld${POSTFIX})
set(CMAKE_CXX_LINKER ${ARM_CODE_GENERATION_TOOLS_DIR}/bin/${PREFIX}-ld${POSTFIX})
set(CMAKE_OBJCOPY ${ARM_CODE_GENERATION_TOOLS_DIR}/bin/${PREFIX}-objcopy${POSTFIX})

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH ${ARM_LINUX_TOOLCHAIN_CONF_DIR})

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

