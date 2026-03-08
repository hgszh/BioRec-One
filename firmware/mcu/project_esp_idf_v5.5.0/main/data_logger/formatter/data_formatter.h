#ifndef DATA_FORMATTER_H_
#define DATA_FORMATTER_H_

#include "data_logger.h"

typedef int32_t (*formatter_write_header_t)(int fd, const data_logger_t *logger_status);
typedef int32_t (*formatter_write_data_t)(int fd, const int32_t *samples, uint32_t sample_count);
typedef int32_t (*formatter_update_metadata_t)(int fd, const data_logger_t *logger_status);
typedef int32_t (*formatter_finalize_file_t)(int fd, const data_logger_t *logger_status);

typedef struct {
    const char *file_extension;
    formatter_write_header_t write_header;
    formatter_write_data_t write_data;
     formatter_update_metadata_t update_metadata; 
    formatter_finalize_file_t finalize_file;
} data_formatter_t;

extern const data_formatter_t csv_formatter;
extern const data_formatter_t bdf_formatter;

#endif // DATA_FORMATTER_H_
