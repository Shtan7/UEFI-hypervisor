# *Hypervisor*

## Brief project description

This project is the Type 1 thin hypervisor that runs on UEFI and Intel processors with VT-x and EPT support. 
To demonstrate the full functionality of the program Windows 11 must act as a guest. The project implements the 
functionality of EPT hooks for Windows executable code (the NtCreateFile function was hooked). To test this feature 
create any file with the substring "you_cant_open_me" in its name. If the Windows module was successfully loaded into 
the system process then an access error will be received when trying to open such a file. The hypervisor was tested 
on Windows 11 22H2 22621.963. For other versions of Windows you may need to manually update the signatures for searching 
the SSDT table that is used to find the NtCreateFile address. 

## Implementation features

* The project has the cpp exceptions port.
* The TLSF allocator is used for dynamic memory allocation.
* For hypervisor and EPT hooks testing there is no need to enable Test Signing Mode or disable PatchGuard.
* Building of the project and the code itself are made in such a way that it is possible to use a large part of the 
standard cpp library.
* Output to the serial port using the PRINT macro is used as a debugging method.
* You can build the project using only Visual Studio 2022. There is no need to install the monstrous EDK2 toolchain. 
Everything is already done as the Visual Studio project.

## Building and debugging

Before building the project make sure that you have WDK, NASM installed and that NASM is present in the PATH variable.
The building order is as follows
* edk2 libs
* Zydis
* win_driver which is present in the hypervisor project and already builded in a byte array in "win_driver.hpp". If you want 
to change the win_driver code then you need to build it and convert it to a C array and store it to "win_driver.hpp".
* hypervisor

I used text output to the com port for debugging and IDA Pro with VMWare Workstation. You can connect IDA to the VMWare gdb stub.
Driver loading can be done via UEFI Shell. First, you need to load the hypervisor driver and then the Windows boot loader.
I plan to make a loading process more convenient in the future. You load the hypervisor via a USB stick and the hypervisor loads
the Windows boot manager.

## IMPORTANT NOTE

I only tested the code in a virtual machine. I didn't run it on real hardware, because I only have one computer at the moment.
I have doubts about the operability of the NMI IPIs handling and about the piece of code where I map the Windows driver to the Windows
system process. The mapping is done in the hypervisor root mode. You can't stay at this mode too long, because you can receive the APIC 
timer interrupt late and then get BSOD.

## TO DO list

- [ ] Make automatic launch of the Windows boot manager after successful loading of the hypervisor.

## Links to the cool repositories

https://github.com/DymOK93/KTL - The CPP exceptions port was taken from this repo. This project is the port of the cpp standart library to the
Windows kernel. Check it out!

https://github.com/avakar/vcrtl - The KTL cpp exception port is based on this repository. Avakar did most of the work on 
reverse engineering the implementation of CPP exceptions in MSVC.
