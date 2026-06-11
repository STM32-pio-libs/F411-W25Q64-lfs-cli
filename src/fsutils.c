#include "fsutils.h"

#define FLASH_CS_PORT GPIOA
#define FLASH_CS_PIN  GPIO_PIN_4

extern SPI_HandleTypeDef  spi1;
static W25Q_LFS_Context lfs_ctx;
static struct lfs_config       lfs_cfg;
static W25Q_Config flash;
static char cwd[100] = "/";


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

const char* getcwd(){
    return cwd;
}
