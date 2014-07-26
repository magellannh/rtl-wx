VERSION_STRING = v2.2
BUILD_DATE_STRING=$(shell date +"%m-%d-%Y")

# Uncomment below to build for Netgear R7000 or similar cpu on DD-WRT
# Must be compiled with cross tools (eg hosted on Ubuntu)  
# Target cpu for R7000 is ARMv7 Cortex-A9
# Requires wrt cross toolchain:
#    eg. OpenWrt-Toolchain-bcm53xx-for-arm_cortex-a9-gcc-4.8-linaro_uClibc-0.9.33.2_eabi
# Once you have the toolchain tar, expand it somewhere and set vars below to point to gcc and lib
TARGET_DISPLAY_NAME = Netgear R7000
TOOLCHAIN=/home/ubuntu/toolchain-arm_cortex-a9_gcc-4.8-linaro_uClibc-0.9.33.2_eabi
export STAGING_DIR = . 
CC   = $(TOOLCHAIN)/bin/arm-openwrt-linux-uclibcgnueabi-gcc
CFLAGS = -I. -I$(TOOLCHAIN)/include/libusb-1.0 -Ilibrtlsdr/include
LIBUSB_DIR = $(TOOLCHAIN)/lib
LIBRTLSDR_DIR = librtlsdr/build/src

# Uncomment Below to build for Raspberry Pi (native compile)
#TARGET_DISPLAY_NAME = Raspberry Pi
#CC = gcc
#CFLAGS = -I. -I/usr/include/libusb-1.0 -Ilibrtlsdr/include
#LIBUSB_DIR = .
#LIBRTLSDR_DIR = librtlsdr/build/src

# Netgear WNR3500L running DD-WRT using cross tools on hosted on
# linux.  Target cpu is MIPSEL Requires cross toolchain, 
#   eg. toolchain-mipsel_gcc4.1.2
# NOTE: NOT WORKING - Program had error opening USB device, suspect libusb-1.0
#TARGET_DISPLAY_NAME = WNR3500L
#TOOLCHAIN=/home/ubuntu/toolchain-mipsel_gcc4.1.2
#export STAGING_DIR = . 
#CC   = $(TOOLCHAIN)/bin/mipsel-linux-gcc
#CFLAGS = -I. -I$(TOOLCHAIN)/include/libusb-1.0 -Ilibrtlsdr/include
#LIBUSB_DIR = $(TOOLCHAIN)/lib
#LIBRTLSDR_DIR = librtlsdr/build/src

ODIR=obj

DEPS = rtl-wx.h TagProc.h getopt.h 

_RTLWX_OBJ = rtl-wx.o TagProc.o DataStore.o ConfProc.o Scheduler.o Util.o rtl_433.o getopt.o
RTLWX_OBJ = $(patsubst %,$(ODIR)/%,$(_RTLWX_OBJ))

_RTL433_OBJ = rtl_433_main.o rtl_433.o getopt.o 
RTL433_OBJ = $(patsubst %,$(ODIR)/%,$(_RTL433_OBJ))

SPACE_CHAR :=
SPACE_CHAR +=
TARGET_DISPLAY_NAME_NO_SPACES = $(subst $(SPACE_CHAR),,$(TARGET_DISPLAY_NAME))

$(ODIR)/%.o: %.c $(DEPS)  | ../bin/$(TARGET_DISPLAY_NAME_NO_SPACES) obj
	$(CC) -c -o $@ $< $(CFLAGS)

all: rtl-wx rtl-433

obj:
	mkdir obj

../bin/$(TARGET_DISPLAY_NAME_NO_SPACES):
	mkdir ../bin/$(TARGET_DISPLAY_NAME_NO_SPACES)

rtl-wx: $(RTLWX_OBJ)
	$(CC)  -O3 -o $@ $^ -Wl,-Bstatic -lrtlsdr -Wl,-Bdynamic -lpthread -lusb-1.0 -lm -L$(LIBUSB_DIR) -L$(LIBRTLSDR_DIR)
	sed -e 's/INSERT_BUILD_STRING/$(VERSION_STRING) built on $(BUILD_DATE_STRING)/g' -e 's/INSERT_TARGET_STRING/for $(TARGET_DISPLAY_NAME)/g' ../www/misc/header.in > ../www/misc/header.htm
	cp ../www/misc/header.htm ../bin/$(TARGET_DISPLAY_NAME_NO_SPACES)/header.htm
	cp rtl-wx ../bin/$(TARGET_DISPLAY_NAME_NO_SPACES)/rtl-wx
	cp rtl-wx ../bin/rtl-wx

rtl-433: $(RTL433_OBJ)
	$(CC)    -Wall -Wextra -Wno-unused -Wsign-compare -g3 -O0 -fvisibility=hidden -o $@ $^ -Wl,-Bstatic -lrtlsdr -Wl,-Bdynamic,-rpath,$(LDIR): -lpthread -lusb-1.0 -lm -L$(LIBUSB_DIR) -L$(LIBRTLSDR_DIR)
	cp rtl-433 ../bin/$(TARGET_DISPLAY_NAME_NO_SPACES)/rtl-433
	cp rtl-433 ../bin/rtl-433

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o

