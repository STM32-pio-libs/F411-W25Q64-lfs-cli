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
static uint32_t mismatch_count = 0;


int open_serial(const char *port, int baud){
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) < 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    /* Control flags: 8N1, no modem control, enable receiver */
    tty.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tty.c_cflag |= CS8 | CLOCAL | CREAD;

    /* Input flags: raw binary, no newline translation */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR | ISTRIP);

    /* Output flags: raw, no post-processing */
    tty.c_oflag &= ~(OPOST | ONLCR);

    /* Local flags: raw mode, no echo */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

    /* Blocking read, one byte minimum */
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) < 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

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

static uint32_t crc33_stm32(const uint8_t *data, size_t len){
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


static bool send_packet(uint8_t *packet, int fd){
    uint32_t board_crc, our_crc;

    packet_count++;
    uint32_t *words = (uint32_t *)packet;
    words[32] = packet_count; 

    write_all(fd, (uint8_t *)packet, 128);
    read_all(fd, (uint8_t *)&board_crc, 4);
    our_crc = crc33_stm32(packet, 128+4);
    if(board_crc == our_crc){
        printf("SENT PACKET %ld: ", packet_count);
        dump_buf(packet, 128+4);
        printf("Board CRC : 0x%08X\n", board_crc);
        printf("Our CRC   : 0x%08X\n", our_crc);
        printf("Match     : YES\n");
    }
    else{
        printf("SENT PACKET %ld: ", packet_count);
        dump_buf(packet, 128+4);
        printf("Board CRC : 0x%08X\n", board_crc);
        printf("Our CRC   : 0x%08X\n", our_crc);
        printf("Match     : NO\n");
        mismatch_count++;
    }
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

    uint8_t packet[128+4] = {0};
    sprintf(packet, "%s", filename);
    send_packet(packet, ufd);

    ssize_t n;
    while ((n = read(ffd, packet, 128)) > 0) {
        send_packet(packet, ufd);
    }

    close(ufd);
    close(ffd);
    printf("Report %ld mismatch out of %ld packets\n", mismatch_count, packet_count);
    return 0;
}
