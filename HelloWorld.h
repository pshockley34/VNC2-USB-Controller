/*
** Filename: HelloWorld.h
**
** Automatically created by Application Wizard 1.2.4.1
**
** Part of solution Samples in project HelloWorld
**
** Comments:
**
** Important: Sections between markers "FTDI:S*" and "FTDI:E*" will be overwritten by
** the Application Wizard
*/
#include "vos.h"

/* FTDI:SHF Header Files */
#include "USB.h"
#include "USBHost.h"
#include "FAT.h"
#include "msi.h"
#include "BOMS.h"
#include "UART.h"
#include "IOCTL.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"
/* FTDI:EHF */

/* FTDI:SDC Driver Constants */
#define VOS_DEV_USBHOST_2		0
#define VOS_DEV_FAT_FILE_SYSTEM 1
#define VOS_DEV_BOMS			2
#define VOS_DEV_UART			3

#define VOS_NUMBER_DEVICES		4
/* FTDI:EDC */

/* FTDI:SDH Driver Handles */
VOS_HANDLE hUSBHOST_2;                 // USB Host Port 2
VOS_HANDLE hFAT_FILE_SYSTEM;           // FAT File System for FAT32 and FAT16
VOS_HANDLE hBOMS;                      // Bulk Only Mass Storage for USB disks
VOS_HANDLE hUART;					   // UART Driver Handle
/* FTDI:EDH */

