#include "fsutils.h"

#define FLASH_CS_PORT GPIOA
#define FLASH_CS_PIN  GPIO_PIN_4

extern SPI_HandleTypeDef  spi1;
static W25Q_LFS_Context lfs_ctx;
static struct lfs_config       lfs_cfg;
static W25Q_Config flash;
static char cwd[100] = "/";
static uint32_t packet_count = 0;

static inline size_t min(size_t a, size_t b){
    return (a < b) ? a : b;
}

static int32_t flash_spi_transfer(void          *user_context,
                                  const uint8_t *tx,
                                  uint8_t       *rx,
                                  size_t         length)
{
    SPI_HandleTypeDef *spi = (SPI_HandleTypeDef *)user_context;
 
    if ((spi == NULL) || (tx == NULL) || (rx == NULL) || (length == 0U)) {
        return W25Q_ERR_INVALID_ARG;
    }
 
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
    HAL_StatusTypeDef result = HAL_SPI_TransmitReceive(spi,
                                                        (uint8_t *)tx, rx,
                                                        (uint16_t)length,
                                                        HAL_MAX_DELAY);
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
 
    return (result == HAL_OK) ? W25Q_OK : W25Q_ERR_IO;
}

static void delay_ms(void *user_context, uint32_t delay_ms){
    (void)user_context;
    HAL_Delay(delay_ms);
}

lfs_t setup_lfs(){
    GPIO_InitTypeDef cs_gpio = {
        .Pin = FLASH_CS_PIN, .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL, .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
    };
    HAL_GPIO_Init(FLASH_CS_PORT, &cs_gpio);
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
 
    flash = (W25Q_Config){
        .transfer_fn  = flash_spi_transfer,
        .delay_ms_fn  = delay_ms,
        .user_context = &spi1
    };
 
    uint8_t mfr, mem_type, capacity;
    if (W25Q_ReadJEDECID(&flash, &mfr, &mem_type, &capacity) != W25Q_OK) {
        printf("Flash not responding\r\n");
        Error_Handler();
    }
    printf("JEDEC: %02X %02X %02X\r\n", mfr, mem_type, capacity);
 
    /* ------------------------------------------------------------------
     * LittleFS setup
     *
     * W25Q_LFS_Context holds the three working buffers (read cache, prog
     * cache, lookahead bitmap).  Declared static so it lives in BSS —
     * lfs requires these buffers to stay alive while the fs is mounted.
     * ------------------------------------------------------------------ */
 
    if (W25Q_LFS_BuildConfig(&flash, &lfs_ctx, &lfs_cfg) != W25Q_OK) {
        printf("LFS config failed\r\n");
        Error_Handler();
    }
 
    /* ------------------------------------------------------------------
     * Mount — format once if the chip has never been formatted
     * ------------------------------------------------------------------ */
    lfs_t lfs;
    int   err = lfs_mount(&lfs, &lfs_cfg);
 
    if (err != LFS_ERR_OK) {
        printf("Mount failed (%d), formatting...\r\n", err);
 
        err = lfs_format(&lfs, &lfs_cfg);
        if (err != LFS_ERR_OK) {
            printf("Format failed (%d)\r\n", err);
            Error_Handler();
        }
 
        err = lfs_mount(&lfs, &lfs_cfg);
        if (err != LFS_ERR_OK) {
            printf("Mount after format failed (%d)\r\n", err);
            Error_Handler();
        }
    }
 
    printf("LFS mounted\r\n");
    
    return lfs;
}

void listdir(lfs_t *lfs, const char* dirname){
    lfs_dir_t dir;
    struct lfs_info info;

    lfs_dir_open(lfs, &dir, dirname);

    while (lfs_dir_read(lfs, &dir, &info) > 0) {
        if(strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;
        if(info.type == LFS_TYPE_REG) printf("r\t");
        else if(info.type == LFS_TYPE_DIR) printf("d\t");
        printf("%lu\t%s\n", info.size, info.name);
    }

    lfs_dir_close(lfs, &dir);
}

void makedir(lfs_t *lfs, const char *dirname){
    int err = lfs_mkdir(lfs, dirname);

    if (err == LFS_ERR_OK) {
        printf("Directory created: %s\r\n", dirname);
    }
    else if (err == LFS_ERR_EXIST) {
        printf("Directory already exists: %s\r\n", dirname);
    }
    else {
        printf("Failed to create directory '%s' (err=%d)\r\n",
               dirname, err);
    }
}

void pathjoin(char* buf, const char* a, const char* b){
    strcpy(buf, a);
    if(a[strlen(a)-1] != '/') strcat(buf, "/");
    strcat(buf, b);
}

const char* getcwd(){
    return cwd;
}

void changedir(lfs_t *lfs, const char* dir){
    if(strcmp(dir, ".") == 0) return;
    if(strcmp(dir, "..") == 0){
        int pos = strlen(cwd)-1;
        while(pos>1 && cwd[pos] != '/') pos--;
        cwd[pos] = '\0';
        return;
    }

    char newpath[100];
    struct lfs_info info;
    pathjoin(newpath, cwd, dir);
    int err = lfs_stat(lfs, newpath, &info);

    if (err < 0) {
        printf("cd: %s: No such file or directory\r\n", newpath);
        return;
    }

    if (info.type != LFS_TYPE_DIR) {
        printf("cd: %s: Not a directory\r\n", newpath);
        return;
    }

    strncpy(cwd, newpath, sizeof(cwd) - 1);
    cwd[sizeof(cwd) - 1] = '\0';

    printf("Current directory: %s\r\n", cwd);
}

void cat(lfs_t *lfs, const char *path){
    struct lfs_info info;
    lfs_file_t file;

    int err = lfs_stat(lfs, path, &info);

    if (err < 0) {
        printf("cat: %s: No such file\r\n", path);
        return;
    }

    if (info.type != LFS_TYPE_REG) {
        printf("cat: %s: Not a regular file\r\n", path);
        return;
    }

    err = lfs_file_open(lfs, &file, path, LFS_O_RDONLY);
    if (err < 0) {
        printf("cat: %s: Cannot open (err=%d)\r\n",
               path, err);
        return;
    }

    char buf[64];
    lfs_ssize_t n;

    while ((n = lfs_file_read(lfs, &file, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, (size_t)n, stdout);
    }

    lfs_file_close(lfs, &file);

    if (n < 0) {
        printf("\r\ncat: Read error (%ld)\r\n", (long)n);
    }
}

void touch(lfs_t *lfs, const char *path){
    struct lfs_info info;
    lfs_file_t file;

    int err = lfs_stat(lfs, path, &info);

    if (err == LFS_ERR_OK) {
        if (info.type == LFS_TYPE_DIR) {
            printf("touch: %s: Is a directory\r\n", path);
            return;
        }

        printf("touch: %s already exists\r\n", path);
        return;
    }

    err = lfs_file_open(lfs, &file, path, LFS_O_WRONLY | LFS_O_CREAT);

    if (err < 0) {
        printf("touch: cannot create %s (err=%d)\r\n",
               path, err);
        return;
    }

    char line[1000];
    printf("Keep typing your text, when done type exact \"EOF\" on a new line\n\r\n\r");
    int linecount = 0;
    while(true){
        fgets(line, sizeof(line), stdin);
        if(strncmp(line, "EOF", 3) == 0) break;
        lfs_file_write(lfs, &file, line, strlen(line));
        linecount++;
    }
    printf("Written %d lines to new file %s\n", linecount);

    lfs_file_close(lfs, &file);

    printf("Created %s\r\n", path);
}

static uint32_t crc33_stm32(const uint8_t *data){
    CRC->CR = CRC_CR_RESET;
    uint32_t *words = (uint32_t *)data;
    for (int i = 0; i < 33; i++) {
        CRC->DR = words[i];
    }
    uint32_t crc = CRC->DR;
    return crc;
}


static bool recv_packet(uint8_t *packet, UART_HandleTypeDef *huart){
    HAL_StatusTypeDef status = HAL_UART_Receive(huart, packet, 128, 500);
    if (status == HAL_TIMEOUT){
        uint32_t crc = TIMEOUT_ERR;
        HAL_UART_Transmit(huart, (uint8_t *)&crc, 4, HAL_MAX_DELAY);
        return false;
    }
    packet_count++;
    uint32_t *words = (uint32_t *)packet;
    words[32] = packet_count; 
    return true;
}

static void ack_packet(uint8_t *packet, UART_HandleTypeDef *huart){
    uint32_t crc = crc33_stm32(packet);
    HAL_UART_Transmit(huart, (uint8_t *)&crc, 4, HAL_MAX_DELAY);
}

void receive_file(lfs_t *lfs, UART_HandleTypeDef *huart){
    __HAL_RCC_CRC_CLK_ENABLE();
    packet_count = 0;

    printf("Receiving files\n\r");
    uint8_t start_pattern[4] = {0x95, 0x54, 0x95, 0x54};
    HAL_UART_Transmit(huart, (uint8_t *)start_pattern, 4, HAL_MAX_DELAY);

    uint8_t packet[128+4];
    struct lfs_info info;
    lfs_file_t file;

    if(!recv_packet(packet, huart)) return;
    packet[24] = '\0';

    uint32_t filesize = ((uint32_t*)packet)[0];
    char filename[21];
    memcpy(filename, packet+4, 20);
    filename[20] = '\0';

    if (lfs_stat(lfs, filename, &info) == LFS_ERR_OK) {
        uint32_t reject = EXISTS_ERR;
        HAL_UART_Transmit(huart, (uint8_t *)&reject, 4, HAL_MAX_DELAY);
        return;
    }
    if (lfs_file_open(lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT) < 0) {
        uint32_t reject = CREATE_ERR;
        HAL_UART_Transmit(huart, (uint8_t *)&reject, 4, HAL_MAX_DELAY);
        return;
    }

    ack_packet(packet, huart);

    while(recv_packet(packet, huart)){
        lfs_file_write(lfs, &file, packet, min(128, (int32_t)filesize));
        filesize -= 128;
        ack_packet(packet, huart);
    }

    lfs_file_close(lfs, &file);
}
