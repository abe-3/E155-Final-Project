// STM32L432KC_SD.h
// Header for SD Card and FAT32 functions

#ifndef STM32L4_SD_H
#define STM32L4_SD_H

#include <stdint.h>
#include "STM32L432KC_GPIO.h"
#include "STM32L432KC_SPI.h"

// FAT32 Definitions
#define SECTOR_SIZE 512

// File structure to keep track of playback
typedef struct {
    uint32_t startCluster;
    uint32_t currentCluster;
    uint32_t size;
    uint32_t sectorsRead;
    uint32_t sectorInCluster;
    uint8_t  sectorsPerCluster;
    uint32_t fatStartLba;
    uint32_t dataStartLba;
} AudioFile;

// Function Prototypes
int SD_Init(void);
int SD_ReadSector(uint32_t sector, uint8_t* buff);
int FAT32_Init(void);
int FAT32_FindFile(const char* name, const char* ext, AudioFile* fileInfo);
int FAT32_ReadNextSector(AudioFile* file, uint8_t* buffer);

#endif