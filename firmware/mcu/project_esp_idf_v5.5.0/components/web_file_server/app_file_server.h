#ifndef __FILE_SERVER_H__
#define __FILE_SERVER_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN)
/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE (4096ULL * 1024 * 1024) // 4096 MB
#define MAX_FILE_SIZE_STR "4096MB"
/* Scratch buffer size */
#define SCRATCH_BUFSIZE (32 * 1024)
// Maximum number of files for which details (e.g., size) are fetched to prevent performance issues
#define FILE_DETAILS_FETCH_THRESHOLD (300)

#include "esp_err.h"
#include <stdbool.h>
esp_err_t start_file_server(char *path);
esp_err_t stop_file_server(void);
bool acquire_file_server_lock(void);
void release_file_server_lock(void);

#ifdef __cplusplus
}
#endif

#endif // __WIFI_APP_H__