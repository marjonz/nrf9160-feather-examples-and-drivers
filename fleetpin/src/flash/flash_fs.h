#ifndef FLASH_FS
#define FLASH_FS

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes the flash filesystem
 */
int flash_fs_init(void);

/**
 * @brief Look for the filename in the filesystem
 * @return - true if the file is already existing, false otherwise.
 */
bool flash_fs_is_file_exist(char * filename);

/**
 * @brief API to write the data to a file
 * @param filename - filename to write to
 * @param data_to_write - pointer to the data to be written to the file.
 * @param data_len - size of the the data to write to file.
 * @return 0 = succesful, errno number otherwise.
 */
int flash_fs_write_file_to_fs(char * filename, const uint8_t * const data_to_write, size_t data_len);

#endif