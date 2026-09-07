/*
 * bl_process.h
 *
 *  Created on: Jan 28, 2025
 *      Author: Adeola
 */

#include "main.h"

#define BOOTLOADER_VERSION_STRING "v1.0\r\n"

#ifndef SRC_BOOTLOADER_BL_PROCESS_H_
#define SRC_BOOTLOADER_BL_PROCESS_H_

#define APP_START_SEC5 		(0x08040000)
#define APP_START_SEC8 		(0x08100000)
#define APP_START_SEC9 		(0x08140000)
#define TOTALFLASHSECTOR	12

#define FIRMWARE_RUN_MEMORY SECTOR	5		//0x08040000
#define FIRMWARE_EXTRNAL_BACKUP_MEMORY 	0		//external memory
#define CLEAR_THEIA_PROGRAM_FLASH SECTOR	8		//0x08100000

#define APP_CHECKS_SEC2 		(0x08010000)
#define APP_CHECKS_SEC2_VER 	(0x08010000+4)	//Address for FW version value
#define APP_CHECKS_SEC2_STA 	(0x08010000+8)	//Address for status of firmware upgrade.

#define APP_CHECKS_SEC3 		(0x08018000)
#define APP_CHECKS_SEC3_VER 	(0x08018000+4)	//Address for FW version value
#define APP_CHECKS_SEC3_STA 	(0x08018000+8)	//Address for status of firmware upgrade.

#define APP_START_SEC11 		(0x081C0000)
#define APP_START_SEC4 			(0x08020000)

#define BOOTLOADER_BACKUP		11
#define BOOTLOADER_STAGE2		4
#define BOOTLOADER_SECTOR1  	0

#define BOOTLOADER_CONTROL_FLAG 0xAA
#define LENGTHCHECK 			100
#define FULL_SECTOR_LEN			0x20000

#define APP_START_SEC1	 		(0x08008000)		//Sector 1

#define INVALID 		(0xFFFFFFFF)
#define STATUS_THEIA_HEX_RCVD		0		//CENOS has received a new firmware from THEIA.
#define STATUS_CENOS_LOAD_SUCCESS	1		//CENOS successfully upgraded with a new firmware
#define STATUS_NEWFW_TO_LOAD		2		//There is new firmware to be loaded by CENOS.

#define FULL_PACKET_LENGTH			1024
typedef void (application_t)(void);

typedef struct
{
    uint32_t		stack_addr;     // Stack Pointer
    application_t*	func_p;        // Program Counter
} JumpStruct;

void bootloaderInit();
void Flash_Read_Data_Byte (uint32_t StartSectorAddress, uint8_t *RxBuf, uint16_t numberofbytes);
void Flash_Read_Data (uint32_t StartSectorAddress, uint32_t *RxBuf, uint16_t numberofwords);
void jumpToApp(const uint32_t address);
void CopyFW(uint8_t fromFW, uint8_t toFW, uint8_t status);
uint8_t EraseSectors(uint8_t SectorFW);
void EraseFWMemory();
//void deinitEverything();
//int CheckLength();


#endif /* SRC_BOOTLOADER_BL_PROCESS_H_ */
