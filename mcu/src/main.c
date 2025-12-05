#include "STM32L432KC.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // for abs()

#define SECTOR_SIZE 512
#define TARGET_NAME "MV"
#define TARGET_EXT  "WAV"

// --- BPB Offsets ---
#define OFF_BPB_BYTES_PER_SEC  0x0B
#define OFF_BPB_SEC_PER_CLUS   0x0D
#define OFF_BPB_RSVD_SEC_CNT   0x0E
#define OFF_BPB_NUM_FATS       0x10
#define OFF_BPB_FAT_SZ_32      0x24
#define OFF_BPB_ROOT_CLUS      0x2C
#define OFF_PART1_LBA_START    0x1C6

#define CS_FPGA_ENABLE()  (GPIOB->BSRR = (1 << (0 + 16))) // PB0 Low
#define CS_FPGA_DISABLE() (GPIOB->BSRR = (1 << 0))        // PB0 High

// --- SD Commands ---
#define CMD0    (0x40+0)    // GO_IDLE_STATE
#define CMD8    (0x40+8)    // SEND_IF_COND
#define CMD17   (0x40+17)   // READ_SINGLE_BLOCK
#define CMD55   (0x40+55)   // APP_CMD
#define CMD58   (0x40+58)   // READ_OCR
#define ACMD41  (0x40+41)   // SD_SEND_OP_COND

// --- SPI chip-select (PA11) ---
#define CS_ENABLE()  (GPIOA->BSRR = (1 << (SPI_CE + 16))) 
#define CS_DISABLE() (GPIOA->BSRR = (1 << SPI_CE))   

// Global sector buffer
static uint8_t buffer[SECTOR_SIZE];

// FAT32 globals
static uint32_t g_lba_begin      = 0;
static uint32_t g_fat_start_lba  = 0;
static uint32_t g_data_start_lba = 0;
static uint32_t g_root_cluster   = 0;
static uint8_t  g_sec_per_clus   = 0;

#define CLUSTER_LBA(c) (g_data_start_lba + ((uint32_t)((c) - 2U) * g_sec_per_clus))

// --- Circular Buffer Config ---
#define DELAY_SECONDS 2
#define BUFFER_SIZE   (16000 * DELAY_SECONDS) 

// Global audio buffer
static uint8_t audio_delay_buffer[BUFFER_SIZE]; 
static uint32_t buffer_head = 0; // Write index (Future/SD)
static uint32_t buffer_tail = 0; // Read index  (Present/DAC)

// --- BEAT DETECTION SETTINGS ---
// SENSITIVITY
// Signal must be 120% louder than average to trigger.
#define SENSITIVITY 1.2f 

// MIN_VOLUME
#define MIN_VOLUME 15     

const uint8_t LANE_MASKS[4] = {0x01, 0x02, 0x04, 0x08};

// State variables for beat detection
static float avg_energy = 20.0f; // Start with a reasonable guess
static int beat_cooldown = 0;

// =====================================================================
// HELPER: Beat Detection & FPGA Trigger
// =====================================================================
void process_beat(uint8_t sample) {
    // 1. Calculate Amplitude (0 to 128)
    int16_t amplitude = (int16_t)sample - 128;
    if (amplitude < 0) amplitude = -amplitude;

    // 2. Dynamic Threshold Logic
    float threshold = avg_energy * SENSITIVITY;

    if (amplitude > threshold && amplitude > MIN_VOLUME) {
        
        if (beat_cooldown == 0) {
            // BEAT DETECTED!
            uint8_t packet = 0;
            int p_idx = sample % 4; 
            
            // Basic beat
            packet |= LANE_MASKS[p_idx];
            
            // REMOVED: Double beat logic.
            // This prevents spawning 2 tiles at once, making it easier to play.
            /* if (amplitude > (threshold * 1.5f)) {
                 packet |= LANE_MASKS[(p_idx + 1) % 4];
            }
            */

            // Send to FPGA
            CS_FPGA_ENABLE(); 
            spiSendReceive(packet);
            CS_FPGA_DISABLE();

            // COOLDOWN INCREASED: 4000 samples @ 16kHz = 250ms.
            // This caps the speed at ~4 beats per second (easier).
            beat_cooldown = 6000; 
        }
    }

    // 3. Update Running Average (Leaky Integrator)
    avg_energy = (avg_energy * 0.999f) + ((float)amplitude * 0.001f);

    // 4. Handle Cooldown
    if (beat_cooldown > 0) {
        beat_cooldown--;
    }
}

// =====================================================================
// SD + SPI 
// =====================================================================

uint8_t SD_Command(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t retry = 0xFF;
    while (spiSendReceive(0xFF) != 0xFF && retry-- > 0);

    spiSendReceive(cmd);
    spiSendReceive((uint8_t)(arg >> 24));
    spiSendReceive((uint8_t)(arg >> 16));
    spiSendReceive((uint8_t)(arg >> 8));
    spiSendReceive((uint8_t)arg);
    spiSendReceive(crc);

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
    int timeout = 10000;
    while (spiSendReceive(0xFF) != 0xFE && timeout-- > 0);
    if (timeout <= 0) {
        CS_DISABLE();
        return -2;
    }
    for (int i = 0; i < SECTOR_SIZE; i++) {
        buff[i] = spiSendReceive(0xFF);
    }
    spiSendReceive(0xFF);
    spiSendReceive(0xFF);
    CS_DISABLE();
    return 0;
}

int SD_Init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    pinMode(SPI_CE, GPIO_OUTPUT);
    CS_DISABLE();
    for (int i = 0; i < 10; i++) spiSendReceive(0xFF);
    CS_ENABLE();
    uint8_t r0 = SD_Command(CMD0, 0, 0x95);
    CS_DISABLE();
    if (r0 != 0x01) return -1;

    CS_ENABLE();
    SD_Command(CMD8, 0x1AA, 0x87);
    spiSendReceive(0xFF); spiSendReceive(0xFF); spiSendReceive(0xFF); spiSendReceive(0xFF);
    CS_DISABLE();

    int retries = 1000;
    uint8_t res;
    do {
        CS_ENABLE();
        SD_Command(CMD55, 0, 0xFF);
        res = SD_Command(ACMD41, 0x40000000, 0xFF);
        CS_DISABLE();
    } while (res != 0x00 && retries-- > 0);

    if (retries <= 0 || res != 0x00) return -2;
    return 0;
}

// =====================================================================
// FAT32 helpers
// =====================================================================

uint32_t get_u32(const uint8_t* b, int offset) {
    return (uint32_t)b[offset] | ((uint32_t)b[offset+1] << 8) | ((uint32_t)b[offset+2] << 16) | ((uint32_t)b[offset+3] << 24);
}

uint16_t get_u16(const uint8_t* b, int offset) {
    return (uint16_t)b[offset] | ((uint16_t)b[offset+1] << 8);
}

int match_filename(const uint8_t* entry, const char* name, const char* ext) {
    for (int i = 0; i < 8; i++) {
        char c = (i < (int)strlen(name)) ? name[i] : ' ';
        if (entry[i] != (uint8_t)c) return 0;
    }
    for (int i = 0; i < 3; i++) {
        char c = (i < (int)strlen(ext)) ? ext[i] : ' ';
        if (entry[8+i] != (uint8_t)c) return 0;
    }
    return 1;
}

uint32_t fat32_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4U;
    uint32_t sector     = g_fat_start_lba + (fat_offset / SECTOR_SIZE);
    uint32_t offset     = fat_offset % SECTOR_SIZE;
    if (SD_ReadSector(sector, buffer) != 0) return 0x0FFFFFFF;
    return get_u32(buffer, offset) & 0x0FFFFFFF;
}

int fat32_mount(void) {
    if (SD_ReadSector(0, buffer) != 0) return -1;
    g_lba_begin = get_u32(buffer, OFF_PART1_LBA_START);
    if (g_lba_begin == 0) return -2;
    if (SD_ReadSector(g_lba_begin, buffer) != 0) return -3;
    g_sec_per_clus = buffer[OFF_BPB_SEC_PER_CLUS];
    uint16_t rsvd_sec = get_u16(buffer, OFF_BPB_RSVD_SEC_CNT);
    uint8_t  num_fats = buffer[OFF_BPB_NUM_FATS];
    uint32_t fat_sz   = get_u32(buffer, OFF_BPB_FAT_SZ_32);
    g_root_cluster    = get_u32(buffer, OFF_BPB_ROOT_CLUS);
    g_fat_start_lba  = g_lba_begin + rsvd_sec;
    g_data_start_lba = g_fat_start_lba + ((uint32_t)num_fats * fat_sz);
    return 0;
}

int fat32_find_root_file(const char* name, const char* ext, uint32_t* first_cluster, uint32_t* file_size) {
    uint32_t cluster_start_lba = CLUSTER_LBA(g_root_cluster);
    for (int sec_offset = 0; sec_offset < g_sec_per_clus; sec_offset++) {
        uint32_t current_lba = cluster_start_lba + sec_offset;
        if (SD_ReadSector(current_lba, buffer) != 0) return -1;
        for (int i = 0; i < SECTOR_SIZE; i += 32) {
            uint8_t first = buffer[i];
            if (first == 0x00) return -2;
            if (first == 0xE5) continue;
            uint8_t attr = buffer[i+11];
            if (attr == 0x0F) continue;
            if (attr & 0x08) continue;
            if (match_filename(&buffer[i], name, ext)) {
                uint32_t clus_hi = get_u16(buffer, i + 20);
                uint32_t clus_lo = get_u16(buffer, i + 26);
                *first_cluster   = (clus_hi << 16) | clus_lo;
                *file_size       = get_u32(buffer, i + 28);
                return 0;
            }
        }
    }
    return -2;
}

// =====================================================================
// WAV header parse 
// =====================================================================

typedef struct {
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint16_t num_channels;
    uint32_t data_offset;
    uint32_t data_size;
} WavInfo;

int parse_wav_header(const uint8_t* sec0, uint32_t file_size, WavInfo* w) {
    if (file_size < 44) return -1;
    if (memcmp(sec0, "RIFF", 4) != 0) return -2;
    if (memcmp(sec0 + 8, "WAVE", 4) != 0) return -3;
    uint32_t offset = 12;
    int have_fmt = 0; int have_data = 0;
    while (offset + 8 <= SECTOR_SIZE) {
        const uint8_t* ch = sec0 + offset;
        uint32_t id = get_u32(ch, 0);
        uint32_t size = get_u32(ch, 4);
        uint32_t data_off = offset + 8;
        if (id == 0x20746D66) { // "fmt "
            w->num_channels = get_u16(sec0, data_off + 2);
            w->sample_rate = get_u32(sec0, data_off + 4);
            w->bits_per_sample = get_u16(sec0, data_off + 14);
            have_fmt = 1;
        } else if (id == 0x61746164) { // "data"
            w->data_offset = offset + 8;
            w->data_size = size;
            if (w->data_offset + w->data_size > file_size) {
                if (file_size > w->data_offset) w->data_size = file_size - w->data_offset;
                else w->data_size = 0;
            }
            have_data = 1;
        }
        offset += 8 + size;
        if (offset >= SECTOR_SIZE) break;
        if (have_fmt && have_data) break;
    }
    if (!have_fmt || !have_data) return -6;
    return 0;
}

// =====================================================================
// DAC & Timer
// =====================================================================

void initDAC(void) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_DAC1EN;
    RCC->AHB2ENR  |= RCC_AHB2ENR_GPIOAEN;
    GPIOA->MODER &= ~(0b11U << (5 * 2));
    GPIOA->MODER |=  (0b11U << (5 * 2));
    DAC1->CR &= ~DAC_CR_TEN2;
    DAC1->CR |= DAC_CR_EN2;
}

void initAudioTimer(uint32_t sample_rate) {
    if (sample_rate == 0) sample_rate = 16000;
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;
    TIM6->CR1 = 0;
    TIM6->PSC = 0;
    TIM6->ARR = (80000000U / sample_rate) - 1U;
    TIM6->EGR = TIM_EGR_UG;
    TIM6->SR  = 0;
    TIM6->CR1 |= TIM_CR1_CEN;
}

static inline void audio_wait_tick(void) {
    while ((TIM6->SR & TIM_SR_UIF) == 0) {}
    TIM6->SR &= ~TIM_SR_UIF;
}

// =====================================================================
// PLAY WAV
// =====================================================================

int play_wav(void) {
    uint32_t file_clus = 0;
    uint32_t file_size = 0;
    if (fat32_find_root_file(TARGET_NAME, TARGET_EXT, &file_clus, &file_size) != 0) {
        printf("File not found.\n"); return -1;
    }

    uint32_t cluster = file_clus;
    uint32_t sector_in_cluster = 0;
    SD_ReadSector(CLUSTER_LBA(cluster), buffer); 
    WavInfo w;
    if (parse_wav_header(buffer, file_size, &w) != 0) return -1;
    
    uint32_t bytes_left_in_file = w.data_size;
    uint32_t sd_buffer_idx = w.data_offset; 
    
    printf("Buffering %d seconds...\n", DELAY_SECONDS);
    
    // --- 3. PRIME BUFFER ---
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (bytes_left_in_file == 0) break;
        if (sd_buffer_idx >= SECTOR_SIZE) {
            sd_buffer_idx = 0;
            sector_in_cluster++;
            if (sector_in_cluster >= g_sec_per_clus) {
                cluster = fat32_next_cluster(cluster);
                if (cluster >= 0x0FFFFFF8) break; 
                sector_in_cluster = 0;
            }
            SD_ReadSector(CLUSTER_LBA(cluster) + sector_in_cluster, buffer);
        }
        uint8_t sample = buffer[sd_buffer_idx++];
        bytes_left_in_file--;
        
        // Use the new helper function
        process_beat(sample);

        audio_delay_buffer[i] = sample;
    }
    
    buffer_head = 0; 
    buffer_tail = 0; 

    // --- 4. START PLAYBACK ---
    printf("Starting Playback.\n");
    initDAC();
    initAudioTimer(w.sample_rate);

    while (bytes_left_in_file > 0) {
        if (sd_buffer_idx >= SECTOR_SIZE) {
            sd_buffer_idx = 0;
            sector_in_cluster++;
            if (sector_in_cluster >= g_sec_per_clus) {
                cluster = fat32_next_cluster(cluster);
                if (cluster >= 0x0FFFFFF8) break;
                sector_in_cluster = 0;
            }
            SD_ReadSector(CLUSTER_LBA(cluster) + sector_in_cluster, buffer);
        }
        uint8_t new_sample = buffer[sd_buffer_idx++];
        bytes_left_in_file--;

        // Use the same helper function here
        process_beat(new_sample);

        uint8_t audio_out = audio_delay_buffer[buffer_tail];
        audio_delay_buffer[buffer_tail] = new_sample;
        buffer_tail++;
        if (buffer_tail >= BUFFER_SIZE) buffer_tail = 0;

        audio_wait_tick();
        DAC1->DHR8R2 = audio_out; 
    }

    // --- 5. DRAIN BUFFER ---
    for (int i = 0; i < BUFFER_SIZE; i++) {
        uint8_t audio_out = audio_delay_buffer[buffer_tail];
        buffer_tail++;
        if (buffer_tail >= BUFFER_SIZE) buffer_tail = 0;
        audio_wait_tick();
        DAC1->DHR8R2 = audio_out;
    }

    DAC1->DHR8R2 = 0x80;
    return 0;
}

int main(void) {
    configureFlash();
    configureClock();
    RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN);

    GPIOB->MODER &= ~(3U << 0);
    GPIOB->MODER |=  (1U << 0); 
    CS_FPGA_DISABLE();          

    initSPI(7, 0, 0);
    if (SD_Init() != 0) return -1;
    SPI1->CR1 &= ~SPI_CR1_SPE;
    SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | (3 << 3); 
    SPI1->CR1 |= SPI_CR1_SPE;
    if (fat32_mount() != 0) return -1;

    play_wav();

    while (1) {}
}