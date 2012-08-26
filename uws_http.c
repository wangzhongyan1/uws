#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include "uws.h"
#include "uws_http.h"
#include "uws_utils.h"
#include "uws_config.h"
#include "uws_mime.h"
#include "uws_header.h"

struct response header_body;
static char* mime;

int
comparestr(const void *p1, const void *p2)
{
    return strcmp(* (char * const *)p1, * (char * const *) p2);
}
void
printdir(const char *fpath) {//打印目录项排序
    DIR *dp = opendir(fpath);
    struct dirent *dir_entry;
    int dir_len = 0;
    char **entries;
    struct stat stat_buff;

    while((dir_entry = readdir(dp)) != NULL)
        dir_len++;

    header_body.content_len = dir_len * 64;//Max filename Length
    header_body.content = (char*) calloc (header_body.content_len, sizeof(char));

    rewinddir(dp);
    entries = (char**) malloc ((dir_len + 5) * sizeof(char*));
    dir_len = 0;
    //
    //判断是否是目录
    //
    
    while((dir_entry = readdir(dp)) != NULL) {
        char *newpath = (char*) calloc(128, sizeof(char));//max filename length
        strcpy(newpath, fpath);

        entries[dir_len++] = dir_entry->d_name;

        //strcat(newpath, "/");
        strcat(newpath, dir_entry->d_name);
        lstat(newpath, &stat_buff);
        if(S_ISDIR(stat_buff.st_mode)) {
            strcat(entries[dir_len - 1], "/");
        }
        free(newpath);
    }
    entries[dir_len] = NULL;
    qsort(entries, dir_len, sizeof(char*), comparestr);


    while(*(entries)!= NULL) {
        strcat(header_body.content, "<a href=\"");
        strcat(header_body.content, *entries);
        strcat(header_body.content, "\">");
        strcat(header_body.content, *(entries++));
        strcat(header_body.content, "</a><br/>\n");
    }
    header_body.content_len = strlen(header_body.content);
    closedir(dp);
}
void
printfile(const char *path)
{
    FILE* file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    header_body.content_len = ftell(file);

    rewind(file);

    header_body.content = (char*) calloc (header_body.content_len, sizeof(char));
    fread(header_body.content, sizeof(char), header_body.content_len, file);
    fclose(file);
}

static char*
get_mime(const char* path)
{
    int i = 0;
    i = strlen(path);
    i--;
    while(*(path + i) != '.') {
        if(i == 0) {
            return strdup("text/html");
            break;
        }
        i--;
    }
    if(i != 0){
        return mimebyext(path + i + 1);
    }
    return strdup("text/html");
}
static void
set_header() {
    char *time_string = get_time_string();
    response_header->http_ver = "HTTP/1.1";
    response_header->status_code = 200;
    response_header->status = "OK";
    add_header_param("Cache-Control", "private", response_header);
    add_header_param("Connection", "Keep-Alive", response_header);
    add_header_param("Server", "UWS/0.001", response_header);
    add_header_param("Date", time_string, response_header);
    add_header_param("Content-Length", itoa(header_body.content_len), response_header);
    add_header_param("Content-Type", mime, response_header);
    header_body.header = str_response_header(response_header);
    header_body.header_len = strlen(header_body.header);
    free(time_string);
}
int
http_router(int sockfd) 
{
    char path[PATH_LEN];
    struct stat stat_buff;
    int i = 0; 
    strcpy(path, request_header->path);
    while(path[i] != 0) {
        if(path[i] == '?' || path[i] == '#') {
            path[i] = 0;
            break;
        }
        i++;
    }
    if(lstat(path, &stat_buff) != -1) {
        if( S_ISDIR(stat_buff.st_mode) ) {
            mime = strdup("text/html");
            printdir(path);
        }
        else
        {
            mime = get_mime(path);
            printfile(path);
        }
    }
    else {
        send_error_response(sockfd, 404);
        return 0;
    }
    set_header();
    int res;
    res = write(sockfd, header_body.header, header_body.header_len);
    res = write(sockfd, HEADER_SEP, strlen(HEADER_SEP));
    res = write(sockfd, header_body.content, header_body.content_len);
    free(header_body.header);
    free(header_body.content);
    free_header_params(response_header);
    return 0;
}
