#include "main.h"
#include <stdio.h>

UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef  spi1;

int main(){
    HAL_Init();
    SystemClock_Config();
    enable_gpio();
    setup_hardfault_led();
    setup_uart1();

    uint32_t x = 0;

    while(1){
        printf("Hello %ld\n", x++); 
        HAL_Delay(100);
    }

}
