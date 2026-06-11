#include "main.h"
#include <stdio.h>

extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef  spi1;

void setup_hardfault_led(void){
    GPIO_InitTypeDef led_init = {.Pin = GPIO_PIN_13,
                                 .Mode = GPIO_MODE_OUTPUT_PP,
                                 .Pull = GPIO_NOPULL,
                                 .Speed = GPIO_SPEED_FREQ_LOW};

    HAL_GPIO_Init(GPIOC, &led_init);
    // pc13 led is active low
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
}

void enable_gpio(void){
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
}


int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

void setup_uart1(){
    __HAL_RCC_USART1_CLK_ENABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitTypeDef uart_pin = {.Pin = GPIO_PIN_9|GPIO_PIN_10,
                                 .Mode = GPIO_MODE_AF_PP,
                                 .Pull = GPIO_NOPULL,
                                 .Speed = GPIO_SPEED_FREQ_VERY_HIGH,
                                 .Alternate = GPIO_AF7_USART1};

    HAL_GPIO_Init(GPIOA, &uart_pin);

    huart1.Instance = USART1;
    huart1.Init = (UART_InitTypeDef){
        .BaudRate = 115200, .WordLength = UART_WORDLENGTH_8B, 
        .StopBits = UART_STOPBITS_1, .Parity = UART_PARITY_NONE, 
        .Mode = UART_MODE_TX_RX, .HwFlowCtl = UART_HWCONTROL_NONE, 
        .OverSampling = UART_OVERSAMPLING_16};

    if (HAL_UART_Init(&huart1) != HAL_OK){
        Error_Handler();
    }
}

void setup_spi1(void){
    __HAL_RCC_SPI1_CLK_ENABLE();

    /**SPI1 GPIO Configuration
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI
    */

    GPIO_InitTypeDef spi_pin = {
        .Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7,
        .Mode = GPIO_MODE_AF_PP, .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_VERY_HIGH, .Alternate = GPIO_AF5_SPI1
    };
    HAL_GPIO_Init(GPIOA, &spi_pin);
 
    spi1.Instance = SPI1;
    spi1.Init = (SPI_InitTypeDef){
        .Mode = SPI_MODE_MASTER, .Direction = SPI_DIRECTION_2LINES,
        .DataSize = SPI_DATASIZE_8BIT, .CLKPolarity = SPI_POLARITY_LOW,
        .CLKPhase = SPI_PHASE_1EDGE, .NSS = SPI_NSS_SOFT,
        .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16,
        .FirstBit = SPI_FIRSTBIT_MSB, .TIMode = SPI_TIMODE_DISABLE,
        .CRCCalculation = SPI_CRCCALCULATION_DISABLE, .CRCPolynomial = 7
    };
    if (HAL_SPI_Init(&spi1) != HAL_OK) { Error_Handler(); }
}

void Error_Handler(void){
    __disable_irq();
    while (1){
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    }
}

void SystemClock_Config(void){
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
    */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 200;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK){
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK){
        Error_Handler();
    }
}


void SysTick_Handler(void){
    HAL_IncTick();
}
