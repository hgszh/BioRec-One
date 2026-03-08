#include "app_file_server.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/param.h>
#include <time.h>

struct file_server_data {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE]; // Scratch buffer for temporary storage during file transfer
};
SemaphoreHandle_t busy_mutex; // Mutex to protect against concurrent file operation
static const char *TAG = "file_server";

static size_t url_decode(char *dst, const char *src, size_t dst_size) {
    size_t dst_len = 0;
    const char *src_ptr = src;
    char *dst_ptr = dst;
    while (*src_ptr && dst_len < dst_size - 1) {
        if (*src_ptr == '%' && src_ptr[1] && src_ptr[2]) {
            if (isxdigit((unsigned char)src_ptr[1]) && isxdigit((unsigned char)src_ptr[2])) {
                uint8_t hex1 =
                    isdigit((unsigned char)src_ptr[1]) ? src_ptr[1] - '0' : tolower((unsigned char)src_ptr[1]) - 'a' + 10;
                uint8_t hex2 =
                    isdigit((unsigned char)src_ptr[2]) ? src_ptr[2] - '0' : tolower((unsigned char)src_ptr[2]) - 'a' + 10;
                *dst_ptr++ = (char)((hex1 << 4) | hex2);
                src_ptr += 3;
                dst_len++;
            } else {
                *dst_ptr++ = *src_ptr++;
                dst_len++;
            }
        } else if (*src_ptr == '+') {
            *dst_ptr++ = ' ';
            src_ptr++;
            dst_len++;
        } else {
            *dst_ptr++ = *src_ptr++;
            dst_len++;
        }
    }
    *dst_ptr = '\0';
    return dst_len;
}

static size_t url_encode(char *dst, const char *src, size_t dst_size) {
    size_t dst_len = 0;
    const char *src_ptr = src;
    char *dst_ptr = dst;
    while (*src_ptr && dst_len < dst_size - 1) {
        char c = *src_ptr;
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            *dst_ptr++ = c;
            dst_len++;
        } else {
            if (dst_len + 3 >= dst_size) {
                break;
            }
            uint8_t high = (c >> 4) & 0x0F;
            uint8_t low = c & 0x0F;
            *dst_ptr++ = '%';
            *dst_ptr++ = (high < 10) ? ('0' + high) : ('A' + high - 10);
            *dst_ptr++ = (low < 10) ? ('0' + low) : ('A' + low - 10);
            dst_len += 3;
        }
        src_ptr++;
    }
    *dst_ptr = '\0';
    return dst_len;
}

/* Embedded file handlers (index, favicon, styles, upload page)  */
static esp_err_t index_html_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t favicon_get_handler(httpd_req_t *req) {
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[] asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

static esp_err_t styles_get_handler(httpd_req_t *req) {
    extern const unsigned char styles_css_start[] asm("_binary_styles_css_start");
    extern const unsigned char styles_css_end[] asm("_binary_styles_css_end");
    const size_t styles_css_size = (styles_css_end - styles_css_start);
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)styles_css_start, styles_css_size);
    return ESP_OK;
}

static esp_err_t upload_page_get_handler(httpd_req_t *req) {
    extern const unsigned char upload_html_start[] asm("_binary_upload_html_start");
    extern const unsigned char upload_html_end[] asm("_binary_upload_html_end");
    const size_t upload_html_size = (upload_html_end - upload_html_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)upload_html_start, upload_html_size);
    return ESP_OK;
}

typedef struct {
    char name[CONFIG_FATFS_MAX_LFN + 1]; // 文件名
    int type;                            // 类型 (DT_DIR 或 DT_REG)
    int ecg_index;                       // 从文件名中解析出的ECG序号
    off_t size;                          // 文件大小
} file_entry_t;
// qsort所需的比较函数
// 1. 文件夹永远排在文件前面。
// 2. 对于ECG文件，按照序号从大到小排列。
// 3. 其他文件和无法解析序号的ECG文件，按字母顺序排列在所有排序后的ECG文件之后。
static int compare_files_desc(const void *a, const void *b) {
    const file_entry_t *entry_a = (const file_entry_t *)a;
    const file_entry_t *entry_b = (const file_entry_t *)b;
    // 规则1：文件夹优先
    if (entry_a->type == DT_DIR && entry_b->type != DT_DIR) return -1; // a (文件夹) 在前
    if (entry_a->type != DT_DIR && entry_b->type == DT_DIR) return 1;  // b (文件夹) 在前
    // 如果两者都是文件夹，或者都不是ECG文件，则按名称排序
    if ((entry_a->type == DT_DIR && entry_b->type == DT_DIR) || (entry_a->ecg_index < 0 && entry_b->ecg_index < 0)) {
        return strcasecmp(entry_a->name, entry_b->name);
    }
    // 规则2：ECG文件按序号降序排序
    // 将有有效序号的文件排在无有效序号的文件之前
    if (entry_a->ecg_index >= 0 && entry_b->ecg_index < 0) return -1;
    if (entry_a->ecg_index < 0 && entry_b->ecg_index >= 0) return 1;
    // 如果两个都是有效的ECG文件，比较它们的序号
    if (entry_a->ecg_index > entry_b->ecg_index) return -1; // a的序号大，排在前面 (降序)
    if (entry_a->ecg_index < entry_b->ecg_index) return 1;  // b的序号大，排在前面 (降序)
    // 如果序号相同，则按文件名排序作为备用规则
    return strcasecmp(entry_a->name, entry_b->name);
}
static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to stat dir : %s", dirpath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
        return ESP_FAIL;
    }
    // 快速读取所有条目, 但不获取大小
    file_entry_t *entries = NULL;
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 动态扩展数组
        file_entry_t *new_entries =
            heap_caps_realloc(entries, (count + 1) * sizeof(file_entry_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!new_entries) {
            ESP_LOGW(TAG, "Failed to allocate memory in PSRAM, trying internal RAM...");
            new_entries = heap_caps_realloc(entries, (count + 1) * sizeof(file_entry_t), MALLOC_CAP_DEFAULT);
        }
        if (!new_entries) {
            ESP_LOGE(TAG, "Failed to allocate memory for directory entries");
            free(entries);
            closedir(dir);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        entries = new_entries;
        file_entry_t *current_entry = &entries[count];
        strlcpy(current_entry->name, entry->d_name, sizeof(current_entry->name));
        current_entry->type = entry->d_type;
        current_entry->ecg_index = -1;
        current_entry->size = 0; // 初始化大小为0
        if (entry->d_type != DT_DIR && strncasecmp(entry->d_name, "ECG", 3) == 0) {
            sscanf(entry->d_name + 3, "%d", &current_entry->ecg_index);
        }
        count++;
    }
    closedir(dir);
    if (count > 0 && count <= FILE_DETAILS_FETCH_THRESHOLD) { // 如果文件总数小于阈值, 才去获取每个文件的具体大小
        for (size_t i = 0; i < count; i++) {
            if (entries[i].type == DT_DIR) continue; // 只为文件类型获取大小，跳过目录
            char entrypath[FILE_PATH_MAX];
            int len = snprintf(entrypath, sizeof(entrypath), "%s/%s", dirpath, entries[i].name);
            if (len < 0 || len >= sizeof(entrypath)) continue; // 路径太长, 大小保持为0
            struct stat entry_stat;
            if (stat(entrypath, &entry_stat) == 0) entries[i].size = entry_stat.st_size;
        }
    }
    // 对内存中的列表进行排序
    if (count > 0) qsort(entries, count, sizeof(file_entry_t), compare_files_desc);
    // 遍历排序后的列表并生成HTML
    extern const unsigned char file_list_1_start[] asm("_binary_file_list_1_html_start");
    extern const unsigned char file_list_1_end[] asm("_binary_file_list_1_html_end");
    const size_t file_list_1_size = (file_list_1_end - file_list_1_start);
    extern const unsigned char file_list_2_start[] asm("_binary_file_list_2_html_start");
    extern const unsigned char file_list_2_end[] asm("_binary_file_list_2_html_end");
    const size_t file_list_2_size = (file_list_2_end - file_list_2_start);
    httpd_resp_send_chunk(req, (const char *)file_list_1_start, file_list_1_size);
    for (size_t i = 0; i < count; i++) {
        const char *entrytype = (entries[i].type == DT_DIR) ? "directory" : "file";
        char encoded_filename[CONFIG_FATFS_MAX_LFN * 3 + 1];
        url_encode(encoded_filename, entries[i].name, sizeof(encoded_filename));
        httpd_resp_sendstr_chunk(req, "<tr><td class=\"");
        httpd_resp_sendstr_chunk(req, entrytype);
        httpd_resp_sendstr_chunk(req, "\"><a href=\"");
        httpd_resp_sendstr_chunk(req, req->uri);
        if (req->uri[strlen(req->uri) - 1] != '/') httpd_resp_sendstr_chunk(req, "/");
        httpd_resp_sendstr_chunk(req, encoded_filename);
        if (entries[i].type == DT_DIR) {
            httpd_resp_sendstr_chunk(req, "/\">");
            httpd_resp_sendstr_chunk(req, entries[i].name);
            httpd_resp_sendstr_chunk(req, "</a></td><td>---</td><td></td></tr>\n");
        } else {
            httpd_resp_sendstr_chunk(req, "\">");
            httpd_resp_sendstr_chunk(req, entries[i].name);
            httpd_resp_sendstr_chunk(req, "</a></td><td>");
            // 根据文件总数决定是否显示大小
            if (count > FILE_DETAILS_FETCH_THRESHOLD) httpd_resp_sendstr_chunk(req, "---"); // 文件太多，不显示大小
            else {
                char entrysize[16];
                double size_kb = (double)entries[i].size / 1024.0;
                snprintf(entrysize, sizeof(entrysize), "%.2f KB", size_kb);
                httpd_resp_sendstr_chunk(req, entrysize); // 文件不多，显示大小
            }
            httpd_resp_sendstr_chunk(req, "</td><td>");
            httpd_resp_sendstr_chunk(req, "<button class=\"deleteButton\" filepath=\"");
            httpd_resp_sendstr_chunk(req, req->uri);
            if (req->uri[strlen(req->uri) - 1] != '/') httpd_resp_sendstr_chunk(req, "/");
            httpd_resp_sendstr_chunk(req, encoded_filename);
            httpd_resp_sendstr_chunk(req, "\">Delete</button>");
            httpd_resp_sendstr_chunk(req, "</td></tr>\n");
        }
    }
    httpd_resp_send_chunk(req, (const char *)file_list_2_start, file_list_2_size);
    httpd_resp_sendstr_chunk(req, NULL);
    free(entries);
    return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename) {
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    } else if (IS_FILE_EXT(filename, ".jpg") || IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".gif")) {
        return httpd_resp_set_type(req, "image/gif");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    } else if (IS_FILE_EXT(filename, ".csv") || IS_FILE_EXT(filename, ".bdf")) {
        return httpd_resp_set_type(req, "application/octet-stream");
    }
    return httpd_resp_set_type(req, "text/plain");
}

static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize) {
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);
    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }
    char *temp_uri = alloca(pathlen + 1);
    strlcpy(temp_uri, uri, pathlen + 1);
    char *decoded_path = alloca(pathlen + 1);
    size_t decoded_len = url_decode(decoded_path, temp_uri, pathlen + 1);
    if (base_pathlen + decoded_len + 1 > destsize) {
        return NULL;
    }
    strcpy(dest, base_path);
    strcpy(dest + base_pathlen, decoded_path);
    return dest + base_pathlen;
}

static esp_err_t download_get_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX];
    int fd = -1;
    struct stat file_stat;
    struct file_server_data *server_data_ctx = (struct file_server_data *)req->user_ctx;

    if (strcmp(req->uri, "/index.html") == 0) return index_html_get_handler(req);
    if (strcmp(req->uri, "/favicon.ico") == 0) return favicon_get_handler(req);
    if (strcmp(req->uri, "/upload.html") == 0) return upload_page_get_handler(req);
    if (strcmp(req->uri, "/styles.css") == 0) return styles_get_handler(req);

    if (xSemaphoreTake(busy_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Server is busy, rejected GET request for %s", req->uri);
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "Server is busy with another file operation.", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    const char *filename = get_path_from_uri(filepath, server_data_ctx->base_path, req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/') {
        esp_err_t ret = http_resp_dir_html(req, filepath);
        xSemaphoreGive(busy_mutex);
        return ret;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    // 设置 Content-Length 响应头
    char len_buf[20];
    snprintf(len_buf, sizeof(len_buf), "%ld", file_stat.st_size);
    httpd_resp_set_hdr(req, "Content-Length", len_buf);
    // 使用文件的最后修改时间 (st_mtime) 来设置 Last-Modified 响应头
    char date_buf[100];
    struct tm timeinfo;
    gmtime_r(&file_stat.st_mtime, &timeinfo);
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &timeinfo);
    httpd_resp_set_hdr(req, "Last-Modified", date_buf);
    set_content_type_from_file(req, filename);
    // 添加 Content-Disposition 响应头
    // 为了直接下载CSV文件，避免浏览器新建标签页预览
    if (IS_FILE_EXT(filename, ".csv") || IS_FILE_EXT(filename, ".bdf")) {
        const char *basename = strrchr(filepath, '/');
        if (basename == NULL) {
            basename = filepath; // 如果没找到'/'，就使用整个路径(虽然不太可能)
        } else {
            basename++; // 跳过'/'字符
        }
        char disposition_hdr[300];
        // 使用提取出的文件名来设置响应头
        snprintf(disposition_hdr, sizeof(disposition_hdr), "attachment; filename=\"%s\"", basename);
        httpd_resp_set_hdr(req, "Content-Disposition", disposition_hdr);
    }

    fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    char *chunk = server_data_ctx->scratch;
    ssize_t read_bytes;

    while (1) {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes < 0) {
            ESP_LOGE(TAG, "Error reading file: %s", filepath);
            close(fd);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            xSemaphoreGive(busy_mutex);
            return ESP_FAIL;
        } else if (read_bytes == 0) {
            break;
        } else {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                xSemaphoreGive(busy_mutex);
                return ESP_FAIL;
            }
        }
    }

    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);

    xSemaphoreGive(busy_mutex);
    return ESP_OK;
}

static esp_err_t upload_post_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX];
    int fd = -1;
    struct stat file_stat;
    struct file_server_data *server_data_ctx = (struct file_server_data *)req->user_ctx;

    if (xSemaphoreTake(busy_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Server is busy, rejected upload request.");
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "Server is busy with another file operation.", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    const char *filename =
        get_path_from_uri(filepath, server_data_ctx->base_path, req->uri + sizeof("/upload") - 1, sizeof(filepath));
    if (!filename) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    if (req->content_len > MAX_FILE_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File size must be less than " MAX_FILE_SIZE_STR "!");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (!fd) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Receiving file : %s...", filename);

    char *buf = server_data_ctx->scratch;
    int received;
    int remaining = req->content_len;

    while (remaining > 0) {
        if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            close(fd);
            unlink(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            xSemaphoreGive(busy_mutex);
            return ESP_FAIL;
        }
        if (received && (received != write(fd, buf, received))) {
            close(fd);
            unlink(filepath);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
            xSemaphoreGive(busy_mutex);
            return ESP_FAIL;
        }
        remaining -= received;
    }

    close(fd);
    ESP_LOGI(TAG, "File reception complete");

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File uploaded successfully");

    xSemaphoreGive(busy_mutex);
    return ESP_OK;
}

static esp_err_t delete_post_handler(httpd_req_t *req) {
    char filepath[FILE_PATH_MAX];
    struct stat file_stat;
    struct file_server_data *server_data_ctx = (struct file_server_data *)req->user_ctx;

    if (xSemaphoreTake(busy_mutex, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Server is busy, rejected delete request for %s", req->uri);
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "Server is busy with another file operation.", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    const char *filename =
        get_path_from_uri(filepath, server_data_ctx->base_path, req->uri + sizeof("/delete") - 1, sizeof(filepath));
    if (!filename) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    if (filename[strlen(filename) - 1] == '/') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File does not exist");
        xSemaphoreGive(busy_mutex);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleting file : %s", filename);
    unlink(filepath);

    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_sendstr(req, "File deleted successfully");

    xSemaphoreGive(busy_mutex);
    return ESP_OK;
}

/**********************************************************************************************************/
static struct file_server_data *server_data = NULL;
static httpd_handle_t server = NULL;

esp_err_t start_file_server(char *path) {
    if (!path) {
        ESP_LOGE(TAG, "base path can't be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, path, sizeof(server_data->base_path));

    if (busy_mutex == NULL) {
        busy_mutex = xSemaphoreCreateMutex();
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 1024 * 5;
    config.task_priority = tskIDLE_PRIORITY + 15;
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_LOGI(TAG, "Starting HTTP Server");
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start file server!");
        vSemaphoreDelete(busy_mutex);
        free(server_data);
        server_data = NULL;
        return ESP_FAIL;
    }

    httpd_uri_t file_download = {.uri = "/*", .method = HTTP_GET, .handler = download_get_handler, .user_ctx = server_data};
    httpd_register_uri_handler(server, &file_download);

    httpd_uri_t file_upload = {.uri = "/upload/*", .method = HTTP_POST, .handler = upload_post_handler, .user_ctx = server_data};
    httpd_register_uri_handler(server, &file_upload);

    httpd_uri_t file_delete = {.uri = "/delete/*", .method = HTTP_POST, .handler = delete_post_handler, .user_ctx = server_data};
    httpd_register_uri_handler(server, &file_delete);

    return ESP_OK;
}

/**********************************************************************************************************/
esp_err_t stop_file_server(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    if (server_data) {
        free(server_data);
        server_data = NULL;
    }
    ESP_LOGI(TAG, "File server stopped");
    return ESP_OK;
}

bool acquire_file_server_lock(void) {
    if (busy_mutex) {
        if (xSemaphoreTake(busy_mutex, 0) == pdTRUE) {
            return true;
        } else {
            return false;
        }
    }
    return true;
}

void release_file_server_lock(void) {
    if (busy_mutex) {
        xSemaphoreGive(busy_mutex);
    }
}
