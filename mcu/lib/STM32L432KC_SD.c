// STM32L432KC_SD.c
// Source for SD Card and FAT32

#include "STM32L432KC_SD.h"
#include "STM32L432KC_RCC.h"
#include <string.h>
#include <stdio.h>

// --- Commands ---
#define CMD0    (0x40+0)    // GO_IDLE_STATE
#define CMD8    (0x40+8)    // SEND_IF_COND
#define CMD17   (0x40+17)   // READ_SINGLE_BLOCK
#define CMD55   (0x40+55)   // APP_CMD
#define ACMD41  (0x40+41)   // SD_SEND_OP_COND

// --- Offsets ---
#define OFF_PART1_LBA_START    0x1C6
#define OFF_BPB_BYTES_PER_SEC  0x0B
#define OFF_BPB_SEC_PER_CLUS   0x0D
#define OFF_BPB_RSVD_SEC_CNT   0x0E
#define OFF_BPB_NUM_FATS       0x10
#define OFF_BPB_FAT_SZ_32      0x24
#define OFF_BPB_ROOT_CLUS      0x2C

#define CS_ENABLE()  (GPIOA->BSRR = (1 << (SPI_CE + 16))) 
#define CS_DISABLE() (GPIOA->BSRR = (1 << SPI_CE))        

// Globals
static uint32_t g_lba_begin = 0;
static uint32_t g_fat_start_lba = 0;
static uint32_t g_data_start_lba = 0;
static uint8_t  g_sec_per_clus = 0;
static uint32_t g_root_cluster = 0;

// --- Low Level Helpers ---

// Change SPI Speed (Divisor: 0=2, 1=4, ... 7=256)
static void SD_SetSpeed(int br) {
    SPI1->CR1 &= ~SPI_CR1_SPE; // Disable
    SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | _VAL2FLD(SPI_CR1_BR, br);
    SPI1->CR1 |= SPI_CR1_SPE;  // Enable
}

static uint8_t SD_Command(uint8_t cmd, uint32_t arg, uint8_t crc) {
    // Wait for ready
    uint8_t retry = 0xFF;
    while(spiSendReceive(0xFF) != 0xFF && retry-- > 0); 

    // Send Command
    spiSendReceive(cmd);
    spiSendReceive((uint8_t)(arg >> 24));
    spiSendReceive((uint8_t)(arg >> 16));
    spiSendReceive((uint8_t)(arg >> 8));
    spiSendReceive((uint8_t)arg);
    spiSendReceive(crc);

    // Wait for response
    uint8_t res = 0;
    for (int i = 0; i < 100; i++) {
        res = spiSendReceive(0xFF);
        if ((res & 0x80) == 0) break; 
    }
    return res;
}

int SD_ReadSector(uint32_t sector, uint8_t* buff) {
    CS_ENABLE();
    if (SD_Command(CMD17, sector, 0xFF) != 0x00) {
        CS_DISABLE();
        return -1; 
    }

    // Wait for data token (0xFE)
    int timeout = 20000;
    while (spiSendReceive(0xFF) != 0xFE && timeout-- > 0);
    if (timeout <= 0) {
        CS_DISABLE();
        return -2;
    }

    // Read Data
    for (int i = 0; i < 512; i++) buff[i] = spiSendReceive(0xFF);
    
    // Read CRC (discard)
    spiSendReceive(0xFF); 
    spiSendReceive(0xFF);

    CS_DISABLE();
    return 0;
}

int SD_Init() {
    pinMode(SPI_CE, GPIO_OUTPUT); 
    CS_DISABLE();

    // 1. Set very slow speed for initialization (< 400kHz)
    // System Clock = 80MHz. Div 256 => ~312kHz
    SD_SetSpeed(0b111); 

    // 2. Power-up delay (80 clocks)
    for (int i = 0; i < 12; i++) spiSendReceive(0xFF);

    // 3. Enter Idle
    CS_ENABLE();
    if (SD_Command(CMD0, 0, 0x95) != 0x01) {
        CS_DISABLE();
        return -1;
    }
    CS_DISABLE();

    // 4. Check Voltage
    CS_ENABLE();
    SD_Command(CMD8, 0x1AA, 0x87);
    // Flush response bytes
    spiSendReceive(0xFF); spiSendReceive(0xFF); spiSendReceive(0xFF); spiSendReceive(0xFF); 
    CS_DISABLE();

    // 5. Init Card (ACMD41)
    int retries = 5000; // Increased retry count
    uint8_t res;
    do {
        CS_ENABLE();
        SD_Command(CMD55, 0, 0xFF);      
        res = SD_Command(ACMD41, 0x40000000, 0xFF); 
        CS_DISABLE();
    } while (res != 0x00 && retries-- > 0);

    if (retries <= 0) return -2;

    // 6. Switch to High Speed
    // Div 4 => 20MHz (Safe for most SPI modules and wiring)
    SD_SetSpeed(0b001); 

    return 0;
}

// --- FAT32 Implementation ---

static uint32_t get_u32(uint8_t* b, int offset) {
    return b[offset] | (b[offset+1] << 8) | (b[offset+2] << 16) | (b[offset+3] << 24);
}
static uint16_t get_u16(uint8_t* b, int offset) {
    return b[offset] | (b[offset+1] << 8);
}

uint32_t ClusterToLBA(uint32_t cluster) {
    return g_data_start_lba + ((cluster - 2) * g_sec_per_clus);
}

int FAT32_Init(void) {
    uint8_t buffer[SECTOR_SIZE];
    
    if(SD_ReadSector(0, buffer) != 0) return -1;
    
    // Simple check for MBR vs Volume Boot Record
    // If byte 0 is 0xEB or 0xE9, it might be a VBR directly (no partition table)
    // Otherwise, assume MBR partition table at offset 0x1C6
    if (buffer[0] != 0xEB && buffer[0] != 0xE9) {
        g_lba_begin = get_u32(buffer, OFF_PART1_LBA_START);
        SD_ReadSector(g_lba_begin, buffer);
    } else {
        g_lba_begin = 0;
    }
    
    g_sec_per_clus = buffer[OFF_BPB_SEC_PER_CLUS];
    uint16_t rsvd_sec = get_u16(buffer, OFF_BPB_RSVD_SEC_CNT);
    uint8_t  num_fats = buffer[OFF_BPB_NUM_FATS];
    uint32_t fat_size = get_u32(buffer, OFF_BPB_FAT_SZ_32);
    g_root_cluster    = get_u32(buffer, OFF_BPB_ROOT_CLUS);

    g_fat_start_lba  = g_lba_begin + rsvd_sec;
    g_data_start_lba = g_fat_start_lba + (num_fats * fat_size);
    
    return 0;
}

static int match_filename(uint8_t* entry, const char* name, const char* ext) {
    for(int i=0; i<8; i++) {
        char c = (i < strlen(name)) ? name[i] : ' ';
        if(entry[i] != c) return 0;
    }
    for(int i=0; i<3; i++) {
        char c = (i < strlen(ext)) ? ext[i] : ' ';
        if(entry[8+i] != c) return 0;
    }
    return 1;
}

int FAT32_FindFile(const char* name, const char* ext, AudioFile* fileInfo) {
    uint8_t buffer[SECTOR_SIZE];
    SD_ReadSector(ClusterToLBA(g_root_cluster), buffer);
    
    for (int i = 0; i < 512; i += 32) {
        if (buffer[i] == 0x00) break; 
        if (buffer[i] == 0xE5) continue; 

        if (match_filename(&buffer[i], name, ext)) {
            uint32_t clusHigh = get_u16(buffer, i + 20);
            uint32_t clusLow  = get_u16(buffer, i + 26);
            
            fileInfo->startCluster = (clusHigh << 16) | clusLow;
            fileInfo->currentCluster = fileInfo->startCluster;
            fileInfo->size = get_u32(buffer, i + 28);
            fileInfo->sectorsPerCluster = g_sec_per_clus;
            fileInfo->sectorInCluster = 0;
            fileInfo->sectorsRead = 0;
            
            return 0; 
        }
    }
    return -1; 
}

int FAT32_ReadNextSector(AudioFile* file, uint8_t* buffer) {
    if (file->sectorsRead * SECTOR_SIZE >= file->size) return -1;

    uint32_t lba = ClusterToLBA(file->currentCluster) + file->sectorInCluster;
    if (SD_ReadSector(lba, buffer) != 0) return -2;

    file->sectorsRead++;
    file->sectorInCluster++;

    // Cluster Boundary Check
    if (file->sectorInCluster >= file->sectorsPerCluster) {
        // Read FAT to find next cluster
        uint32_t fatOffset = file->currentCluster * 4;
        uint32_t fatSector = g_fat_start_lba + (fatOffset / SECTOR_SIZE);
        uint32_t entOffset = fatOffset % SECTOR_SIZE;

        // WARNING: We reuse the data buffer for FAT table read. 
        // This destroys 'buffer' content if we aren't careful, 
        // but here 'buffer' was already filled with data we *just* returned?
        // Actually, the caller expects 'buffer' to contain the file data.
        // We CANNOT overwrite 'buffer' here. We need a temp buffer.
        uint8_t fatBuffer[512]; 
        SD_ReadSector(fatSector, fatBuffer);

        uint32_t nextCluster = get_u32(fatBuffer, entOffset) & 0x0FFFFFFF;
        
        if (nextCluster >= 0x0FFFFFF8) return -1; // EOF
        
        file->currentCluster = nextCluster;
        file->sectorInCluster = 0;
    }

    return 0;
}