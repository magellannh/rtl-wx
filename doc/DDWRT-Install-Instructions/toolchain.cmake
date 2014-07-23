# this one is important 
SET(CMAKE_SYSTEM_NAME Linux) 
#this one not so much 
SET(CMAKE_SYSTEM_VERSION 1) 

# specify the cross compiler
# STAGING_DIR is needed for openwrt toolchain
set(STAGING_DIR ".")
SET(CMAKE_C_COMPILER   /home/ubuntu/toolchain-arm_cortex-a9_gcc-4.8-linaro_uClibc-0.9.33.2_eabi/bin/arm-openwrt-linux-uclibcgnueabi-gcc) 
SET(CMAKE_CXX_COMPILER /home/ubuntu/toolchain-arm_cortex-a9_gcc-4.8-linaro_uClibc-0.9.33.2_eabi2/bin/arm-openwrt-linux-uclibcgnueabi-cpp) 

# where is the target environment 
SET(CMAKE_FIND_ROOT_PATH  /home/ubuntu/toolchain-arm_cortex-a9_gcc-4.8-linaro_uClibc-0.9.33.2_eabi) 
SET(LIBUSB_INCLUDE_DIR /home/ubuntu/toolchain-arm_cortex-a9_gcc-4.8-linaro_uClibc-0.9.33.2_eabi/include/libusb-1.0)
SET(THREADS_PTHREADS_INCLUDE_DIR /home/ubuntu/toolchain-arm_cortex-a9_gcc-4.8-linaro_uClibc-0.9.33.2_eabi/include)

# search for programs in the build host directories 
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) 
# for libraries and headers in the target directories 
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY) 
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
