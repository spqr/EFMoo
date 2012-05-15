
================ EFM32 Device Support Library (DSL) ==========================

This directory ,"CMSIS", contains the Energy Micro Support Library for the 
EFM32G series of microcontrollers.

================ About CMSIS =================================================

The library is based on CMSIS, the Cortex Microcontroller Software Interface 
Standard, as defined by ARM Inc. 

For more information about CMSIS see 
    http://www.onarm.com
    http://www.arm.com/products/CPUs/CMSIS.html

In short, CMSIS tries to provide a common interface for programming devices 
having one of the Cortex-M core architectures, making code sharing and reuse
easier.

================ Development Environments ====================================

You will need a development environment which support the Energy Micro EFM32
devices. Currently this is either

IAR Embedded Workbench for ARM 5.40.6 or later, see http://www.iar.com
Keil uVision "MDK-ARM" 4.01 or later, see http://www.keil.com

It is possible to develop with other tools, but for now these are the best
supported development environment for EFM32 devices.

This library use C99-types, requires the presence of <stdint.h> and can use
other functionality standardized in C99. If your compiler has a C99 compliance
toggle, you should enable it for your projects.

================ File structure ==============================================

Short getting started guide:

The quickest way to start is to base your work on one of the simple example
projects for the EFM32-Gxxx-DK Development Kits. These should be easy to port
and change to adopt to your needs.

Load the project files for IAR from the directory
  CMSIS/CM3/Example/iar/EFM32
or, for Keil from the directory
  CMSIS/CM3/Example/arm/EFM32

Please note that you _will_ need to change the "Debugger" and "Flash/Download"
configuration settings to fit your environment. See your IDE's manuals for 
details.

Support for the EFM32G device family is located in the directory
  CMSIS/CM3/DeviceSupport/EnergyMicro/EFM32

The most convenient way to start a project, is to define the device target
in your compiler options, e.g. add a -DEFM32G890F128 to your compile options
if you are targetting an EFM32G890F128 part.

When this is done, you should include the file "efm32.h" wherever you need 
register and bit field definitions for the EFM32 peripherals.

The peripheral registers follow the CMSIS convention of defining a structure
which holds the "volatile" registers. Again, take a look at the examples for
usage.

================ Licenses ====================================================

See the top of each file for SW license. Basically you are free to use the
Energy Micro code for any project using EFM32 devices. Part of the CMSIS
library is copyrighted by ARM Inc. See "License.doc" for ARM's CMSIS license.

================ Software updates ============================================

Energy Micro continually works to provide updated and improved DSL, example
code and other software of use for EFM32 customers. Please check the download
section of 

        http://www.energymicro.com/

for the latest releases, news and updates. 

               (C) Copyright Energy Micro AS, 2009




