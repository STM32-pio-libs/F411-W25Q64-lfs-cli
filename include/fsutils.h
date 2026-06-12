#ifndef __FSUTILS_H__
#define __FSUTILS_H__
#include "main.h"
#include "w25q64.h"
#include "w25q64_lfs.h"

lfs_t setup_lfs();
const char* getcwd();
void listdir(lfs_t *lfs, const char* dir);
void makedir(lfs_t *lfs, const char *dirname);
void pathjoin(char* buf, const char* a, const char* b);
void changedir(lfs_t *lfs, const char* dir);
void cat(lfs_t *lfs, const char *path);
void touch(lfs_t *lfs, const char *path);
void receive_file(UART_HandleTypeDef *huart1);


#endif //__FSUTILS_H__
