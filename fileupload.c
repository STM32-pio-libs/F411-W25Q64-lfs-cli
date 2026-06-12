#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

static uint8_t our_xor = 0;
static uint32_t packet_count = 0;


int open_serial(const char *port, int baud){
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios tty;
    tcgetattr(fd, &tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |=  CS8;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}

/* Write exactly n bytes — write() can return short */
int write_all(int fd, const uint8_t *buf, size_t n){
    size_t written = 0;
    while (written < n) {
        ssize_t r = write(fd, buf + written, n - written);
        if (r < 0) { perror("write"); return -1; }
        written += r;
    }
    return 0;
}

int write_all_slow(int fd, const uint8_t *buf, size_t n){
    size_t written = 0;
    while (written < n) {
        ssize_t r = write(fd, buf + written, n - written);
        tcdrain(fd);
        usleep(1000);
        if (r < 0) { perror("write"); return -1; }
        written += r;
    }
    return 0;
}

/* Read exactly n bytes — blocks until all arrive */
int read_all(int fd, uint8_t *buf, size_t n){
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, buf + got, n - got);
        if (r < 0) { perror("read"); return -1; }
        got += r;
    }
    return 0;
}

void dump_buf(uint8_t buf[], ssize_t size){
    printf("[");
    for(ssize_t i=0; i<size; i++){
        printf("%x, ", buf[i]);
    }
    printf("]\n");
}

static uint32_t crc32_stm32(const uint8_t *data, size_t len){
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i += 4) {
        /* Little-endian word — matches how STM32 reads from memory into CRC->DR */
        uint32_t word = ((uint32_t)data[i+3] << 24)
                      | ((uint32_t)data[i+2] << 16)
                      | ((uint32_t)data[i+1] <<  8)
                      | ((uint32_t)data[i+0] <<  0);

        for (int b = 0; b < 32; b++) {
            if ((crc ^ word) & 0x80000000U) {
                crc = (crc << 1) ^ 0x04C11DB7U;
            } else {
                crc = (crc << 1);
            }
            word <<= 1;
        }
    }
    return crc;
}

static void xor_data(const uint8_t *data){
    for(int i=0; i<128; i++){
        our_xor ^= data[i];
    }
}

static bool send_packet(uint8_t *packet, int fd){
    packet_count++;
    uint32_t board_crc, our_crc;
    uint8_t board_xor = 0; our_xor;
    write_all(fd, (uint8_t *)packet, 128);
    printf("SENT PACKET %ld: ", packet_count);
    dump_buf(packet, 128);
    read_all(fd, (uint8_t *)&board_crc, 4);
    read_all(fd, &board_xor, 1);
    our_crc = crc32_stm32(packet, 128);
    xor_data(packet);
    printf("Board CRC : 0x%08X\n", board_crc);
    printf("Our CRC   : 0x%08X\n", our_crc);
    printf("Match     : %s\n", board_crc == our_crc ? "YES" : "NO");
    printf("Board XOR : 0x%02X\n", board_xor);
    printf("Our XOR   : 0x%02X\n", our_xor);
    printf("Match     : %s\n", board_xor == our_xor ? "YES" : "NO");
}

void initiate_coms(int fd){
    /* Send the command */
    write_all_slow(fd, "\n\n\n", 3);
    tcdrain(fd);
    const char *cmd = "receive\n";
    write_all_slow(fd, (uint8_t *)cmd, strlen(cmd));
    tcdrain(fd);
    usleep(1000);

    /* read bytes one at a time until we see R-D-Y in sequence */
    uint8_t window[4] = {0};
    for (;;) {
        uint8_t b;
        read(fd, &b, 1);
        window[0] = window[1];
        window[1] = window[2];
        window[2] = window[3];
        window[3] = b;
        if (window[0]==0x95 && window[1]==0x54 && 
            window[2]==0x95 && window[3]==0x54) break;
    }
    tcflush(fd, TCIFLUSH);
}

int main(void){
    setvbuf(stdout, NULL, _IONBF, 0);
    char *port = "/dev/ttyACM0";
    int ufd = open_serial(port, B115200);
    if (ufd < 0) return 1;
    printf("Connected to: %s\n", port);

    initiate_coms(ufd);
    printf("Sending packets: %s\n", port);

    char* filename = "file.tar.gz";
    int ffd = open(filename, O_RDONLY);
    if (ffd < 0) {
        perror("open");
        return 1;
    }

    uint8_t packet[128] = {0};
    sprintf(packet, "%s", filename);
    send_packet(packet, ufd);

    ssize_t n;
    while ((n = read(ffd, packet, 128)) > 0) {
        send_packet(packet, ufd);
    }

    close(ufd);
    close(ffd);
    return 0;
}
