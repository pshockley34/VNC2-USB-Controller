/*
	Main functionality for the USB Controller
	Capstone Design II - Mailbox Group
	
	The USB controller is responsible for reading in the names to be displayed
	on the mailbox LEDs from a csv file and sending them to the microcontroller
	via UART.
	
	Drivers:
		UART - send/recieve data
		USB Host - Interface controller with a USB drive
		BOMS (Bulk Only Mass Storage) - Interface b/w file system and USB Host
		FAT - File system of USB
*/
	
#include "vos.h"
#include "Mailbox.h"

vos_tcb_t* tcbFIRMWARE;

void firmware();
void iomux_setup(void);

/* Main code - entry point to firmware */
void main(void) {
	// UART and USB Host configuration context
	uart_context_t uartContext;            
	usbhost_context_t usbhostContext;   	

	// Kernel Initialization 
	vos_init(50, VOS_TICK_INTERVAL, VOS_NUMBER_DEVICES);
	vos_set_clock_frequency(VOS_48MHZ_CLOCK_FREQUENCY);
	vos_set_idle_thread_tcb_size(256);

	// Pin Setup (default for 32 pin device)
	iomux_setup();
	
	// Initialize UART Driver
	uartContext.buffer_size = VOS_BUFFER_SIZE_128_BYTES;
	uart_init(VOS_DEV_UART, &uartContext);

	// Initialize FAT File System Driver
	fatdrv_init(VOS_DEV_FAT_FILE_SYSTEM);

	// Initialize BOMS Device Driver
	boms_init(VOS_DEV_BOMS);

	// Initialize USB Host Driver
	usbhostContext.if_count = 8;
	usbhostContext.ep_count = 16;
	usbhostContext.xfer_count = 2;
	usbhostContext.iso_xfer_count = 2;
	usbhost_init(-1, VOS_DEV_USBHOST_2, &usbhostContext);

	// Create Thread
	tcbFIRMWARE = vos_create_thread_ex(29, 4096, firmware, "firmware", 0);

	// Pass program control to Kernel Scheduler
	vos_start_scheduler();

main_loop:
	goto main_loop;
}

// Attach FAT File system to BOMS driver (MSI, Mass Storage Interface)
VOS_HANDLE fat_attach(VOS_HANDLE hMSI, unsigned char devFAT) {
	fat_ioctl_cb_t fat_ioctl;
	fatdrv_ioctl_cb_attach_t fat_att;
	VOS_HANDLE hFAT;

	// currently the MSI (BOMS or other) must be open
	// open the FAT driver
	hFAT = vos_dev_open(devFAT);

	// attach the FAT driver to the MSI driver
	fat_ioctl.ioctl_code = FAT_IOCTL_FS_ATTACH;
	fat_ioctl.set = &fat_att;
	fat_att.msi_handle = hMSI;
	fat_att.partition = 0;

	if (vos_dev_ioctl(hFAT, &fat_ioctl) != FAT_OK) {
		// unable to open the FAT driver
		vos_dev_close(hFAT);
		hFAT = NULL;
	}
	return hFAT;
}

void fat_detach(VOS_HANDLE hFAT) {
	fat_ioctl_cb_t fat_ioctl;

	if (hFAT) {
		fat_ioctl.ioctl_code = FAT_IOCTL_FS_DETACH;
		fat_ioctl.set = NULL;
		fat_ioctl.get = NULL;

		vos_dev_ioctl(hFAT, &fat_ioctl);
		vos_dev_close(hFAT);
	}
}

// Attach BOMS to USB Host Driver
VOS_HANDLE boms_attach(VOS_HANDLE hUSB, unsigned char devBOMS) {
	usbhost_device_handle_ex ifDisk;
	usbhost_ioctl_cb_t hc_iocb;
	usbhost_ioctl_cb_class_t hc_iocb_class;
	msi_ioctl_cb_t boms_iocb;
	boms_ioctl_cb_attach_t boms_att;
	VOS_HANDLE hBOMS;

	// find BOMS class device
	hc_iocb_class.dev_class = USB_CLASS_MASS_STORAGE;
	hc_iocb_class.dev_subclass = USB_SUBCLASS_MASS_STORAGE_SCSI;
	hc_iocb_class.dev_protocol = USB_PROTOCOL_MASS_STORAGE_BOMS;

	// user ioctl to find first hub device
	hc_iocb.ioctl_code = VOS_IOCTL_USBHOST_DEVICE_FIND_HANDLE_BY_CLASS;
	hc_iocb.handle.dif = NULL;
	hc_iocb.set = &hc_iocb_class;
	hc_iocb.get = &ifDisk;

	if (vos_dev_ioctl(hUSB, &hc_iocb) != USBHOST_OK) {
		return NULL;
	}

	// USB device found, initialize BOMS
	hBOMS = vos_dev_open(devBOMS);

	// perform attach
	boms_att.hc_handle = hUSB;
	boms_att.ifDev = ifDisk;

	boms_iocb.ioctl_code = MSI_IOCTL_BOMS_ATTACH;
	boms_iocb.set = &boms_att;
	boms_iocb.get = NULL;

	if (vos_dev_ioctl(hBOMS, &boms_iocb) != MSI_OK) {
		vos_dev_close(hBOMS);
		hBOMS = NULL;
	}
	return hBOMS;
}

void boms_detach(VOS_HANDLE hBOMS) {
	msi_ioctl_cb_t boms_iocb;

	if (hBOMS) {
		boms_iocb.ioctl_code = MSI_IOCTL_BOMS_DETACH;
		boms_iocb.set = NULL;
		boms_iocb.get = NULL;

		vos_dev_ioctl(hBOMS, &boms_iocb);
		vos_dev_close(hBOMS);
	}
}

// Get the USB connect state and return to see if enumerated
unsigned char usbhost_connect_state(VOS_HANDLE hUSB) {
	unsigned char connectstate = PORT_STATE_DISCONNECTED;
	usbhost_ioctl_cb_t hc_iocb;

	if (hUSB) {
		hc_iocb.ioctl_code = VOS_IOCTL_USBHOST_GET_CONNECT_STATE;
		hc_iocb.get = &connectstate;
		vos_dev_ioctl(hUSB, &hc_iocb);

		// repeat if connected to see if we move to enumerated
		if (connectstate == PORT_STATE_CONNECTED) {
			vos_dev_ioctl(hUSB, &hc_iocb);
		}
	}
	return connectstate;
}

// Open UART and USB Host drivers
void open_drivers(void) {
	hUSBHOST_2 = vos_dev_open(VOS_DEV_USBHOST_2);
	hUART = vos_dev_open(VOS_DEV_UART);
}

// Attach BOMS driver to USB Host and FAT driver to BOMS	
void attach_drivers(void) {
	hBOMS = boms_attach(hUSBHOST_2, VOS_DEV_BOMS);
	hFAT_FILE_SYSTEM = fat_attach(hBOMS, VOS_DEV_FAT_FILE_SYSTEM);
}

// Close UART and USB Host drivers
void close_drivers(void) {
	vos_dev_close(hUSBHOST_2);
	vos_dev_close(hUART);
}

/* Application Threads */

void firmware() {
	// Mailbox USB Reading Variables
	FILE* stream;
	int i, letter;
	int index = 0;
	char name[60]; 
	int countNames = 0;
	
	open_drivers();

	// Setup the UART interface
	uart_iocb.ioctl_code = VOS_IOCTL_COMMON_ENABLE_DMA;
	vos_dev_ioctl(hUART, &uart_iocb);

	// Set baud rate
	uart_iocb.ioctl_code = VOS_IOCTL_UART_SET_BAUD_RATE;
	uart_iocb.set.uart_baud_rate = UART_BAUD_115200;
	vos_dev_ioctl(hUART, &uart_iocb);

	// Set flow control
	uart_iocb.ioctl_code = VOS_IOCTL_UART_SET_FLOW_CONTROL;
	uart_iocb.set.param = UART_FLOW_RTS_CTS;
	vos_dev_ioctl(hUART, &uart_iocb);

	// Set data bits
	uart_iocb.ioctl_code = VOS_IOCTL_UART_SET_DATA_BITS;
	uart_iocb.set.param = UART_DATA_BITS_8;
	vos_dev_ioctl(hUART, &uart_iocb);

	// Set stop bits
	uart_iocb.ioctl_code = VOS_IOCTL_UART_SET_STOP_BITS;
	uart_iocb.set.param = UART_STOP_BITS_1;
	vos_dev_ioctl(hUART, &uart_iocb);

	// Set parity
	uart_iocb.ioctl_code = VOS_IOCTL_UART_SET_PARITY;
	uart_iocb.set.param = UART_PARITY_NONE;
	vos_dev_ioctl(hUART, &uart_iocb);

	// Allow UART to read/write to stdio
	stdioAttach(hUART);

	do {
		// Wait for enumeration to complete
		vos_delay_msecs(1000);
		
		if (usbhost_connect_state(hUSBHOST_2) == PORT_STATE_ENUMERATED) {
			hBOMS = boms_attach(hUSBHOST_2, VOS_DEV_BOMS);
			vos_delay_msecs(1000);

			hFAT_FILE_SYSTEM = fat_attach(hBOMS, VOS_DEV_FAT_FILE_SYSTEM);
			vos_delay_msecs(1000);

			// Attach the stdio file system to the FAT file system
			fsAttach(hFAT_FILE_SYSTEM);
			
			// Try and open file, could be any of these names
			stream = fopen("mailbox.csv", "r");
			if (stream == NULL) stream = fopen("Mailbox.csv", "r");
			if (stream == NULL) stream = fopen("mailboxes.csv", "r");
			if (stream == NULL) stream = fopen("Mailboxes.csv", "r");
			
			// Read names from csv and send to Microcontroller through UART
			do {
				if (stream == NULL || feof(stream)) break;  // Null file or end of file, exit loop
				letter = fgetc(stream);                     // Read character from file
				if (letter == 0xA) continue;                // 0xA is new line in hex, skip new line characters
				if (letter == 0xD) {                        // 0xD is carriage return in hex, so you know when the full name is read
					for (i = index; i < 60; ++i) {      // For some reason this IDE initializes all strings to be vos- repeating.
						name[i] = '\0';             // This loop changes the remaining vos-'s in the string to empty characters.
					}
					printf("%s", name);                 // Send name to Microcontroller through UART   
					index = 0;                          // Reset index for next name
					continue;
				}
				name[index] = letter;                       // Place character in name
				index++;
			} while (1);
			
			// Finished reading names, close file and detach drivers
			fclose(stream);
			fat_detach(hFAT_FILE_SYSTEM);
			boms_detach(hBOMS);

			vos_delay_msecs(5000);
		}
	} while (1);
}
