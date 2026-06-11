#include "main.h"
#include <stdio.h>
#include "fsutils.h"

UART_HandleTypeDef huart1;
SPI_HandleTypeDef  spi1;

void print_help(){
    printf("Available commands:\n\r");
    printf("  ls [path]      List files and directories\n\r");
    printf("  cat <file>     Display file contents\n\r");
    printf("  cd <dir>       Change current directory\n\r");
    printf("  pwd            Print current directory\n\r");
    printf("  help [cmd]     Show help information\n\r");
}

void runcmd(char* cmd){
    int pos = 0;
    int len = strlen(cmd);
    while(pos < len && cmd[pos] != ' '&& cmd[pos] != '\n' && cmd[pos] != '\r'){pos++;}
    int pos2 = pos+1;
    while(pos2 < len && cmd[pos2] != ' '&& cmd[pos2] != '\n' && cmd[pos2] != '\r'){pos2++;}
    cmd[pos2] = '\0';
    char* cmd2 = cmd+pos+1;

    if(strncmp(cmd, "ls", 2) == 0){
        printf("ls command\n\r");
        printf("next: **%s**\n\r", cmd2);
    }
    else if(strncmp(cmd, "cat", 3) == 0){
        printf("cat command\n\r");
    }
    else if(strncmp(cmd, "cd", 2) == 0){
        printf("cd command\n\r");
    }
    else if(strncmp(cmd, "pwd", 2) == 0){
        printf("CWD: %s\n\r", getcwd());
    }
    else if(strncmp(cmd, "help", 2) == 0){
        print_help();
    }
    else{
        printf("Unknown command! run \"help\"\n\r");
    }
}

void print_banner(void){
    printf(
        "\r\n\r\n"
        "    ___ __  __  __     ___________    ________    ____\n\r"
        "   / (_) /_/ /_/ /__  / ____/ ___/   / ____/ /   /  _/\n\r"
        "  / / / __/ __/ / _ \\/ /_   \\__ \\   / /   / /    / /  \n\r"
        " / / / /_/ /_/ /  __/ __/  ___/ /  / /___/ /____/ /   \n\r"
        "/_/_/\\__/\\__/_/\\___/_/    /____/   \\____/_____/___/  \n\r"
        "\n\r"
    );
}

int main(){
    HAL_Init();
    SystemClock_Config();
    enable_gpio();
    setup_hardfault_led();
    setup_uart1();
    setup_spi1();
    setvbuf(stdout, NULL, _IONBF, 0);

    print_banner();
    lfs_t lfs = setup_lfs();
    char s[100];

    while (1) { 
        printf("%s# ", getcwd());
        fgets(s, sizeof(s), stdin);
        runcmd(s);
    }

}

