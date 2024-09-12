## Variable-grained Control Flow Integrity for Programmable Logic Controller

This work was implemented on Raspberry Pi CM4 with OpenPLC_v3, Arm-Trusted-Firmware, and Linux kernel 5.8.y.
#### Step 0: install rpi raspbian
This work utilized 64-bit Bullseys desptop full 2.3G 2023-12-05
```
sudo rpi-imager
```
Please mount rpi4 in your PC
```
sudo apt install git libusb-1.0-0-dev pkg-config build-essential
git clone --depth=1 https://github.com/raspberrypi/usbboot
cd usbboot
make
sudo ./rpiboot
```
#### Step 1: install ATF
```
git clone https://github.com/ARM-software/arm-trusted-firmware.git
```
#### Step 2: compile ATF
The document is located at https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/plat/rpi4.rst

if you use rpi 3, please follow https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/plat/rpi3.rst
```
cd arm-trusted-firmware
CROSS_COMPILE=aarch64-linux-gnu- make PLAT=rpi4 DEBUG=1
cp build/rpi4/debug/bl31.bin /media/<path/to/bootfs>
```
Add the following codes at the end of config.txt in /media/<path/to/bootfs>
```
armstub=bl31.bin
enable_uart=1
enable_gic=1
```
#### Step 3: install Linux kernel
```
git clone --depth=1 -b rpi-5.8.y https://github.com/raspberrypi/linux
```
#### Step 4: compile Linux kernel and install modules
```
cd linux
KERNEL=kernel8
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- bcm2711_defconfig
make -j 6 ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
cp arch/arm64/boot/Image /path/to/boot/kernel8.img
cp arch/arm64/boot/dts/broadcom/*.dtb /media/<path/to/bootfs>
cp arch/arm64/boot/dts/broadcom/*.dtb* /media/<path/to/bootfs>/overlays/
sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
INSTALL_MOD_PATH=/media/<path/to/rootfs> modules_install
```
#### Step 5: install OpenPLC_v3
```
git clone https://github.com/thiagoralves/OpenPLC_v3.git
cd OpenPLC_v3
./install.sh linux
```
if you use rpi3, please use `./install.sh rpi3`
#### Step 6: install VCFI
```
git clone https://github.com/VCFI4PLC/VCFI4PLC.git
```
Add the ATF parts:
```
## Please add the corresponding lines "bl31/*.c" into "BL31_SOURCES += ..." in arm-trusted-firmware/bl31/bl31.mk
cp VCFI4PLC/atf/bl31/*.c arm-trusted-firmware/bl31/
cp VCFI4PLC/atf/include/bl31/*.h arm-trusted-firmware/include/bl31/
## Please add "#define OEN_TEEPLC_START    U(7)" and "#define OEN_TEEPLC_END    U(7)" into arm-trusted-firmware/include/lib/smccc.h
## Please add VCFI4PLC/atf/services/std_svc_setup.c into the end of arm-trusted-firmware/services/std_svc/std_svc_setup.c
```
Please re-compile ATF (Step 2)

Add the linux parts:
```
cp -r VCFI4PLC/linux/teeplc linux/drivers/
## Please add 'source "driver/teeplc/Kconfig"' into the end of linux/drivers/Kconfig
## Please add 'obj-$(CONFIG_TEEPLC)   += teeplc/' into the end of linux/drivers/Makefile
```
Please re-compile linux (Step 4)

Replace the OpenPLC_v3 parts:
```
cp -r VCFI4PLC/openplc/webserver/core/* OpenPLC_v3/webserver/core/
cp VCFI4PLC/openplc/webserver/scripts/* OpenPLC_v3/webserver/scripts/
cp VCFI4PLC/openplc/webserver/st_files/* OpenPLC_v3/webserver/st_files/
```

Launch TEEPLC_DEVICE in OpenPLC_v3:
```
cd /usr/lib/modules/5.8.18-v8+/kernel/drivers/teeplc
sudo insmod teeplc.ko
```

Run OpenPLC
```
cd OpenPLC_v3/webserver/
sudo ./scripts/compile_program.sh xxx.st teeplc
sudo ./core/openplc
```

<!--
**VCFI4PLC/VCFI4PLC** is a ✨ _special_ ✨ repository because its `README.md` (this file) appears on your GitHub profile.

Here are some ideas to get you started:

- 🔭 I’m currently working on ...
- 🌱 I’m currently learning ...
- 👯 I’m looking to collaborate on ...
- 🤔 I’m looking for help with ...
- 💬 Ask me about ...
- 📫 How to reach me: ...
- 😄 Pronouns: ...
- ⚡ Fun fact: ...
-->
