/**
 * @file bl_process.c
 * @brief Bootloader process implementation
 * @author BGR
 * @date Jan 28, 2025
 */

#include <bl1_hwdt.h>
#include <bl1_process.h>
#include <string.h>
#include "strings.h"
#include "stdio.h"
#include <inttypes.h>


uint8_t dotlog;

/**
 * @brief This function is the entry point of the application. It will check 
 *        the status of the firmware and decide which firmware to load.
 *
 *        If the firmware version in APP1_CHECKS is 0xFFFFFFFF, it means the 
 *        device is first time loaded, and the firmware in APP2_CHECKS will be
 *        copied to APP1_CHECKS.
 *
 *        If the firmware version in APP1_CHECKS is not 0xFFFFFFFF and the
 *        firmware version in APP3_CHECKS is different from the firmware version
 *        in APP1_CHECKS, it means new firmware is received from THEIA, and the
 *        new firmware will be copied to APP1_CHECKS.
 *
 *        If the firmware version in APP1_CHECKS is not 0xFFFFFFFF and the
 *        firmware version in APP3_CHECKS is the same as the firmware version
 *        in APP1_CHECKS, it means the new firmware is running successfully, and
 *        the firmware in APP3_CHECKS will be copied to APP2_CHECKS.
 *
 *        If the firmware version in APP1_CHECKS is not 0xFFFFFFFF and the
 *        firmware version in APP3_CHECKS is the same as the firmware version
 *        in APP1_CHECKS and the status in APP1_CHECKS is STATUS_NEWFW_TO_LOAD,
 *        it means the new firmware is not working successfully, and the firmware
 *        in APP2_CHECKS will be copied to APP1_CHECKS.
 *
 *        If the firmware version in APP1_CHECKS is not 0xFFFFFFFF and the
 *        firmware version in APP3_CHECKS is the same as the firmware version
 *        in APP1_CHECKS and the status in APP1_CHECKS is not STATUS_NEWFW_TO_LOAD,
 *        it means the firmware in APP1_CHECKS is running successfully, and the
 *        firmware in APP3_CHECKS will be erased.
 *
 *        Finally, it will jump to the address in APP1_START to boot up the CENOS.
 *
 * @param None
 * @retval None
 */

#define CRC32_POLY  0xEDB88320UL   /* standard CRC-32, reflected */
#define CRC32_INIT  0xFFFFFFFFUL

uint32_t crc32_check;
/**
 * @brief  Incremental CRC32 update — call once per packet, carrying
 *         the running CRC forward between calls.
 */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ CRC32_POLY : (crc >> 1);
        }
    }
    return crc;
}
uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

uint32_t verify_crc32(uint32_t length)
{
	uint8_t readBuffer[FULL_PACKET_LENGTH];
	uint32_t StartMemory = APP_START_SEC4+2;
	uint32_t crc_state = CRC32_INIT;      // once, at start of update

	uint16_t full_blocks	= length / FULL_PACKET_LENGTH;
	uint16_t remainder 		= length % FULL_PACKET_LENGTH;

	for (uint16_t i = 0; i < full_blocks; i++)
	{
		for(uint16_t j = 0; j < FULL_PACKET_LENGTH; j++)
		{
			Flash_Read_Data_Byte(StartMemory + ((uint32_t)(i * FULL_PACKET_LENGTH) + j), &readBuffer[j], 0);
		}
		crc_state = crc32_update(crc_state, readBuffer, FULL_PACKET_LENGTH);
	}

	if (remainder != 0)
	{
		for(uint16_t j = 0; j < remainder; j++)
		{
			Flash_Read_Data_Byte(StartMemory + ((uint32_t)full_blocks * FULL_PACKET_LENGTH + j), &readBuffer[j], 0);
		}
		crc_state = crc32_update(crc_state, readBuffer, remainder);
	}

	return crc32_finalize(crc_state);
}

void bootloaderInit()
{
	uint32_t bootloaderCheck;
	uint8_t bootloaderFlag = 0;

	Flash_Read_Data_Byte(APP_START_SEC1 + 8, &bootloaderFlag, 0);
	Flash_Read_Data(APP_START_SEC4, &bootloaderCheck, 0);
	Flash_Read_Data(APP_START_SEC1, &crc32_check, 0);

	uint32_t bootloaderLength;
	Flash_Read_Data(APP_START_SEC1 + 4, &bootloaderLength, 0);

	uint32_t crcVAL=0;

	if(bootloaderLength != INVALID)
	{
		crcVAL = verify_crc32(bootloaderLength);
	}


	if( (bootloaderCheck == INVALID) ||  (crcVAL != crc32_check)/*|| BootloaderLength == 0xFFFF*/ )
	{//1st bootup.
		Flash_Read_Data(APP_START_SEC11, &bootloaderCheck, 0);

		if(bootloaderCheck != INVALID )
		{
			CopyFW(BOOTLOADER_BACKUP, BOOTLOADER_STAGE2, 0); //Not only copies but also calculates CRC and clears bootloaderCheck
		}
	}
	else
	{
		if(bootloaderFlag == BOOTLOADER_CONTROL_FLAG)
		{
			if( (crc32_check == INVALID) || (crcVAL  != crc32_check) ) // We might have new Bootloader but wrong checksum, or corrupted bootloader
			{
				CopyFW(BOOTLOADER_BACKUP, BOOTLOADER_STAGE2, 0);
			}
			else
			{
				//CopyFW(BOOTLOADER_STAGE2, BOOTLOADER_BACKUP, 0);
				EraseSectors(BOOTLOADER_SECTOR1);

				HAL_FLASH_Unlock();
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_START_SEC1, crc32_check);
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_START_SEC1+4, bootloaderLength);
				HAL_FLASH_Lock();
			}
		}
	}

	HAL_GPIO_TogglePin(HW_WDI_PORT, HW_WDI_PIN);

	jumpToApp(APP_START_SEC4);
}

/**
 * @brief Copies firmware from one address to another. The start address is the 
 *        value of the fromFW parameter plus the base address of the firmware 
 *        sector. The end address is the value of the toFW parameter plus the base 
 *        address of the firmware sector. The function will erase the sectors at 
 *        the end address before copying the firmware.
 *
 *        The function will also unlock the flash memory and lock it after the copy
 *        is complete. It will also toggle the watchdog pin before and after the
 *        copy.
 *
 * @param fromFW The value of the firmware sector to copy from.
 * @param toFW The value of the firmware sector to copy to.
 * @retval None
 */
void CopyFW(uint8_t fromFW, uint8_t toFW, uint8_t status)
{
	uint32_t StartMemory, EndMemory;

	switch(fromFW)
	{
		case BOOTLOADER_BACKUP:	StartMemory = APP_START_SEC11;	break;
		case BOOTLOADER_STAGE2:	StartMemory = APP_START_SEC4;	break;
		default: break;
	}

	switch(toFW)
	{
		case BOOTLOADER_BACKUP:	EndMemory = APP_START_SEC11;	break;
		case BOOTLOADER_STAGE2:	EndMemory = APP_START_SEC4;		break;
		default: break;
	}

	if ( EraseSectors(toFW) != HAL_OK)
		Error_Handler();

	dotlog = SET;
	uint64_t i = 0;
	HAL_GPIO_TogglePin(HW_WDI_PORT, HW_WDI_PIN);
	HAL_FLASH_Unlock();

	uint8_t readByte, writeByte;

	uint8_t readLengthBuffer[LENGTHCHECK];
	memset(readLengthBuffer, 0x00, LENGTHCHECK);

	uint8_t ffArray[LENGTHCHECK];
	memset(ffArray, 0xFF, LENGTHCHECK);

	uint32_t length = 0;

	for(i=0; i < FULL_SECTOR_LEN; i++)
	{
		Flash_Read_Data_Byte(StartMemory + i, &readByte, 0);

		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, EndMemory + i, readByte);

		Flash_Read_Data_Byte(EndMemory + i, &writeByte, 0);

//		if(writeByte != readByte)
//		{	i--;	}

		if(readByte == 0xFF)
		{
			for(uint32_t j = 0; j < LENGTHCHECK; j++)
			{
				Flash_Read_Data_Byte(StartMemory + i + LENGTHCHECK - j, &readLengthBuffer[j], 0);
			}

			if (memcmp(readLengthBuffer, ffArray, LENGTHCHECK) == 0)
			{
				length = i-2; // 32 consecutive 0xFF is read, this should be the end of the firmware
				break;
			}
		}
	}

	//Calculate CRC
	uint8_t readBuffer[FULL_PACKET_LENGTH];
	StartMemory = APP_START_SEC4+2;
	uint32_t crc_state = CRC32_INIT;      // once, at start of update

	uint16_t full_blocks	= length / FULL_PACKET_LENGTH;
	uint16_t remainder 		= length % FULL_PACKET_LENGTH;

	for (uint16_t i = 0; i < full_blocks; i++)
	{
		for(uint16_t j = 0; j < FULL_PACKET_LENGTH; j++)
		{
			Flash_Read_Data_Byte(StartMemory + ((uint32_t)(i * FULL_PACKET_LENGTH) + j), &readBuffer[j], 0);
		}
		crc_state = crc32_update(crc_state, readBuffer, FULL_PACKET_LENGTH);
	}

	if (remainder != 0)
	{
		for(uint16_t j = 0; j < remainder; j++)
		{
			Flash_Read_Data_Byte(StartMemory + ((uint32_t)full_blocks * FULL_PACKET_LENGTH + j), &readBuffer[j], 0);
		}
		crc_state = crc32_update(crc_state, readBuffer, remainder);
	}

	/* on last packet */
	uint32_t final_crc = crc32_finalize(crc_state);

	/* write final_crc into the image header */
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_START_SEC1, final_crc);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, APP_START_SEC1+4, length);

	HAL_GPIO_TogglePin(HW_WDI_PORT, HW_WDI_PIN);
	dotlog = RESET;
	HAL_FLASH_Lock();

	return;
}

/**
 * @brief Erases the specified sectors of the flash memory based on the input parameter.
 *
 * This function determines the start and end sectors to erase based on the
 * `SectorFW` input parameter. It unlocks the flash memory for writing, then
 * iterates over the specified range of sectors, erasing each one. During the
 * sector erase process, it toggles a watchdog pin before and after each sector
 * erase operation to prevent a watchdog reset. After completing the erasure, the
 * function locks the flash memory to prevent unintended writes.
 *
 * @param SectorFW The value representing the firmware sector group to erase.
 *
 * @retval HAL_OK or HAL_ERROR
 */

uint8_t EraseSectors(uint8_t SectorFW)
{
	uint32_t flashError;
	uint32_t SECTORError;

	static FLASH_EraseInitTypeDef EraseInitStruct;

	uint32_t sectorsToErase[TOTALFLASHSECTOR];
	uint8_t sectorCount = RESET;

	flashError = HAL_OK;
	switch(SectorFW)
	{
		case BOOTLOADER_STAGE2:
			sectorsToErase[sectorCount++] = FLASH_SECTOR_1;
			sectorsToErase[sectorCount++] = FLASH_SECTOR_4;
			break;
			break;
		case BOOTLOADER_BACKUP:
			sectorsToErase[sectorCount++] = FLASH_SECTOR_1;
//			sectorsToErase[sectorCount++] = FLASH_SECTOR_11;
			break;
		case BOOTLOADER_SECTOR1:
			sectorsToErase[sectorCount++] = FLASH_SECTOR_1;
		default:
			HAL_FLASH_Lock();
			return HAL_ERROR;
			break;
	}

	/* Unlock the Flash to enable the flash control register access */
	HAL_FLASH_Unlock();

	// Loop through the sectors to erase them one by one
	for (uint8_t i = RESET; i < sectorCount; i++)
	{
		uint32_t sector = sectorsToErase[i];


		HAL_GPIO_TogglePin(HW_WDI_PORT, HW_WDI_PIN);		//feed watchdog.

		// Fill EraseInit structure for current sector
		EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
		EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
		EraseInitStruct.Sector = sector;
		EraseInitStruct.NbSectors = 1;  // Erase only 1 sector at a time

		if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK)
		{
			flashError = HAL_FLASH_GetError();
			break;
		}

		HAL_GPIO_TogglePin(HW_WDI_PORT, HW_WDI_PIN);	//feed watchdog.
		HAL_Delay(50);
	}

	if (flashError != HAL_OK)
	{
		HAL_FLASH_Lock();
		return HAL_ERROR;
	}

 	HAL_FLASH_Lock();

 	return HAL_OK;
}


/**
 * @brief Reads data from flash
 * @param StartSectorAddress Start address of the flash sector to read from
 * @param RxBuf Pointer to the buffer to store the data
 * @param numberofwords Number of words to read
 */
void Flash_Read_Data (uint32_t StartSectorAddress, uint32_t *RxBuf, uint16_t numberofwords)
{
	while (1)
	{
		*RxBuf = *(__IO uint32_t *)StartSectorAddress;
		StartSectorAddress += 4;
		RxBuf++;
		if (!(numberofwords--)) break;
	}
}

/**
 * @brief Reads data from flash one byte at a time
 * @param StartSectorAddress Start address of the flash sector to read from
 * @param RxBuf Pointer to the buffer to store the data
 * @param numberofbytes Number of bytes to read
 */
void Flash_Read_Data_Byte (uint32_t StartSectorAddress, uint8_t *RxBuf, uint16_t numberofbytes)
{
	if (numberofbytes == 0) numberofbytes = 1;
	for (uint16_t n = 0; n < numberofbytes; n++)
	{
		*RxBuf++ = *(__IO uint8_t *)StartSectorAddress;
		StartSectorAddress += 1;
	}
}

/**
 * @brief Jumps to the application code starting at the specified address.
 *
 * This function prepares the system to jump to a specific application by
 * performing several actions. It first retrieves the application's reset
 * handler from the vector table located at the specified address. It then
 * toggles a GPIO pin to signal the watchdog, deinitializes the hardware
 * abstraction layer (HAL), and resets the system clock. The main stack
 * pointer is set to the value found at the application's start address.
 * Finally, the function jumps to the application's reset handler, effectively
 * transferring control to the application.
 *
 * @param address The start address of the application to jump to.
 */

void jumpToApp(const uint32_t address)
{

	void (*app_reset_handler)( void) = (void*)(*((volatile uint32_t*) (address + 4U)));

	HAL_GPIO_TogglePin(HW_WDI_PORT, HW_WDI_PIN);

	/* Reset the Clock */
	HAL_RCC_DeInit();
	HAL_DeInit();
	__set_MSP(*(volatile uint32_t*) address);
	SysTick->CTRL = RESET;
	SysTick->LOAD = RESET;
	SysTick->VAL = RESET;

	/* Jump to application */
	app_reset_handler();    //call the app reset handler
}



//int CheckLength()
//{
//	uint32_t BootLength;
//	uint8_t readCode[2];
//
//	Flash_Read_Data(APP1_CHECKS, &BootLength, 1);
//	if(BootLength != 0xFFFFFFFF)
//	{
//		Flash_Read_Data_Byte(APP2_START+BootLength-1, &readCode[0], 1);
//		Flash_Read_Data_Byte(APP2_START+BootLength, &readCode[1], 1);
//		if(readCode[0] != 0xFF && readCode[1] == 0xFF)
//		{
//			return 1;
//		}
//		else
//		{
//			return 0;
//		}
//	}
//	else
//	{
//		return 0;
//	}
//}


//void deinitEverything()
//{
//	//-- reset peripherals to guarantee flawless start of user application
//	HAL_RCC_DeInit();
//	HAL_DeInit();
//	SysTick->CTRL = 0;
//	SysTick->LOAD = 0;
//	SysTick->VAL = 0;
//}
