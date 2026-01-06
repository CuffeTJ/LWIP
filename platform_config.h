#ifndef __PLATFORM_CONFIG_H_
#define __PLATFORM_CONFIG_H_

#include "xparameters.h"

#define USE_SOFTETH_ON_ZYNQ 0

#ifdef XPAR_XEMACPS_0_BASEADDR
    #define PLATFORM_EMAC_BASEADDR XPAR_XEMACPS_0_BASEADDR
#else
    #define PLATFORM_EMAC_BASEADDR XPAR_PS7_ETHERNET_0_BASEADDR
#endif

#define PLATFORM_ZYNQ 

#endif
