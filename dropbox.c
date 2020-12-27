#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <jansson.h>
#include <dirent.h>
#include <string.h>

#define OFFSET_CONSTANT 3688
#define MAX_BUFFER 7000000

// CURL *curl;
// CURLcode res;
// struct curl_slist *list = NULL;

typedef struct MemoryStruct
{
    char *memory;
    size_t size;
} memory_struct;

struct Filedata
{
    int inodeoffile;
    time_t modifiedtime;
};

char access_token[1024] =
    {"Authorization: Bearer R57RAvBXW6MAAAAAAAAAAXIqRvd6pz1JdNxWeuh1dmFalpK7ldqUsOWwfFMF3DU-"};
char cnt_type[1024] =
    {"Content-Type: application/octet-stream"};

void set_request_props(CURL *curl, char *url, char *req_type, struct curl_slist *list)
{
    curl_easy_setopt(curl, CURLOPT_URL, url);                /* Set Url */
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req_type); /* Request Type*/
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);        /* Request header*/
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    memory_struct *mem = (memory_struct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL)
    {
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int upload_file(const char *filename)
{
    FILE *fd;
    struct stat file_info;
    curl_off_t speed_upload, total_time;

    CURL *curl;
    CURLcode res;
    struct curl_slist *list = NULL;
    curl = curl_easy_init();

    char *url =
        {"https://content.dropboxapi.com/2/files/upload"};

    //  char *drp_api_arg =
    //    {"Dropbox-API-Arg: {\"path\": \"/temp.txt\",\"mode\": \"add\"\
      //  ,\"autorename\": true,\"mute\": false,\"strict_conflict\": false}"};

    char drp_api_arg[200];

    sprintf(drp_api_arg, "Dropbox-API-Arg: {\"path\": \"/nextgenupload5/%s\",\"mode\": \"add\"\
        ,\"autorename\": true,\"mute\": false,\"strict_conflict\": false}",
            filename);

    fd = fopen(filename, "rb"); /* open file to upload */
    if (!fd)
        return 1; /*file not found*/

    if (fstat(fileno(fd), &file_info) != 0)
        return 1;

    if (curl)
    {
        printf("\nUpload File\n");
        list = curl_slist_append(list, access_token);
        list = curl_slist_append(list, drp_api_arg);
        list = curl_slist_append(list, cnt_type);

        set_request_props(curl, url, "POST", list);

        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_READDATA, fd);
        // curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
        //                  (curl_off_t)file_info.st_size);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        printf("\n");
        curl_easy_cleanup(curl);
        curl_slist_free_all(list);
    }
    fclose(fd);
    return 0;
}

const char *start_session()
{
    json_t *root = NULL;
    json_error_t error;
    memory_struct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    CURL *curl;
    CURLcode res;
    struct curl_slist *list = NULL;
    curl = curl_easy_init();

    const char *session_id;
    char url_get_session[100] = {"https://content.dropboxapi.com/2/files/upload_session/start"};
    const char *drp_arg = "Dropbox-API-Arg: {\"close\": false}";

    if (curl)
    {
        printf("Start Session\n");
        list = curl_slist_append(list, access_token);
        list = curl_slist_append(list, drp_arg);
        list = curl_slist_append(list, cnt_type);

        set_request_props(curl, url_get_session, "POST", list);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 12L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl);
        root = json_loads(chunk.memory, 0, &error);
        session_id = json_string_value(json_object_get(root, "session_id"));

        curl_easy_cleanup(curl);
        curl_slist_free_all(list);
    }
    return session_id;
}

long int file_size(FILE *input)
{

    fseek(input, 0L, SEEK_END);
    long int size = ftell(input);
    fseek(input, 0L, SEEK_SET);
    return size;
}

int end_session(const char *session_id, int offset, const char *filename, char *data)
{

    printf("%s", filename);
    CURL *curl;
    CURLcode res;
    struct curl_slist *list = NULL;
    curl = curl_easy_init();

    char access_token[1024] =
        {"Authorization: Bearer R57RAvBXW6MAAAAAAAAAAXIqRvd6pz1JdNxWeuh1dmFalpK7ldqUsOWwfFMF3DU-"};
    char url_end_session[200] = {"https://content.dropboxapi.com/2/files/upload_session/finish"};

    char drp_arg[200];
    sprintf(drp_arg, "Dropbox-API-Arg: {\"cursor\": {\"session_id\": \"%s\",\"offset\": %d},\"commit\": {\"path\": \"/nextgenupload5/%s\",\"mode\": \"add\",\"autorename\": true,\"mute\": false,\"strict_conflict\": false}}",
            session_id, offset, filename);

    if (curl)
    {
        printf("\nEnd Session\n");
        list = curl_slist_append(list, access_token);
        list = curl_slist_append(list, drp_arg);
        list = curl_slist_append(list, cnt_type);

        set_request_props(curl, url_end_session, "POST", list);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 12L);
        // curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(list);

    return 0;
}

static void append_upload(const char *session_id, int offset, char *data, int bytes_read)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *list = NULL;
    curl = curl_easy_init();

    struct stat file_info;
    curl_off_t speed_upload, total_time;

    char *url_append_file = {"https://content.dropboxapi.com/2/files/upload_session/append_v2"};
    char drp_arg[100];
    sprintf(drp_arg, "Dropbox-API-Arg: {\"cursor\": {\"session_id\": \"%s\",\"offset\": %d},\"close\": false}",
            session_id, offset);

    if (curl)
    {
        list = curl_slist_append(list, access_token);
        list = curl_slist_append(list, drp_arg);
        list = curl_slist_append(list, cnt_type);

        set_request_props(curl, url_append_file, "POST", list);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bytes_read);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(list);
    printf("\n");
}

void upload_large_file(const char *filename)
{

    int bytes_read = 0;
    int total_bytes = 0;
    char buffer[MAX_BUFFER];

    printf("*************************\n%s\n*********************", filename);
    FILE *input = fopen(filename, "rb");

    const char *session_id = start_session();

    int i = 0;
    while ((bytes_read = fread(buffer, 1, MAX_BUFFER, input)) > 0)
    {
        printf("\n%d", bytes_read);
        append_upload(session_id, total_bytes, buffer, bytes_read);
        total_bytes += bytes_read;
        memset(buffer, 0, MAX_BUFFER);
    }
    end_session(session_id, total_bytes, filename, "buffer");
}
int download_file()
{

    return 0;
}
void uploadfiles(const char *fname)
{

    FILE *inputchk = fopen(fname, "rb");

    printf("%ld:%s\n", file_size(inputchk), fname);
    if (file_size(inputchk) > 150000000)
    {
        fclose(inputchk);
        upload_large_file(fname);
    }
    else
    {
        fclose(inputchk);
        printf("\ni am less than 150 mb\n");
        upload_file(fname);
    }
}

static time_t getFileModifiedTime(const char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        // printf("%s: last modified time: %s", path, ctime(&attr.st_mtime));
        return attr.st_mtime;
    }
    return 0;
}
int get_inode(int fd)
{
    struct stat buf;
    int ret;

    ret = fstat(fd, &buf);
    if (ret < 0)
    {
        perror("fstat");
        return -1;
    }

    return buf.st_ino;
}

void chkfiles()
{ //Add files in text file and maintain record
    FILE *inf;

    inf = fopen("filedata2.txt", "r");
    if (inf == NULL)
    {
        inf = fopen("filedata2.txt", "w");
    }

    fclose(inf);
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    int pie = 0;
    if (d)
    {

        while ((dir = readdir(d)) != NULL)
        {
            if (strcmp(dir->d_name, ".") == 0)
            {
            }
            else if (strcmp(dir->d_name, "..") == 0)
            {
            }
            else if (strcmp(dir->d_name, "session") == 0)
            {
            }
            else if (strcmp(dir->d_name, "filedata2.txt") == 0)
            {
            }
            else
            {

                struct Filedata inp;
                inf = fopen("filedata2.txt", "r");
                if (inf == NULL)
                {
                    fprintf(stderr, "\nError to open the file\n");
                    exit(1);
                }
                int fd, inode1, inode2;

                fd = open(dir->d_name, O_RDONLY);
                if (fd < 0)
                {
                    perror("open");
                    return;
                }

                inode1 = get_inode(fd);
                int x = 0;
                while (fread(&inp, sizeof(struct Filedata), 1, inf))
                {
                    struct stat attr;

                    if (inode1 == inp.inodeoffile)
                    {
                        x = 1;
                        if (getFileModifiedTime(dir->d_name) > inp.modifiedtime)
                        {
                            printf("\n%s\n", dir->d_name);
                            x = 0;
                        }
                        //  printf("inode = %d modified = %s\n", inp.inodeoffile, ctime(&(inp.modifiedtime)));
                    }
                }
                fclose(inf);

                if (x == 1)
                {
                    x = 1;
                }
                if (x == 0)
                {
                    FILE *of;
                    of = fopen("filedata2.txt", "a");
                    if (of == NULL)
                    {
                        fprintf(stderr, "\nError to open the file\n");
                        exit(1);
                    }

                    time_t t1 = getFileModifiedTime(dir->d_name);

                    fd = open(dir->d_name, O_RDONLY);
                    if (fd < 0)
                    {
                        perror("open");
                        return;
                    }

                    inode1 = get_inode(fd);
                    printf("%d\n", inode1);

                    struct Filedata inp1 = {inode1, t1};
                    uploadfiles(dir->d_name);
                    fwrite(&inp1, sizeof(struct Filedata), 1, of);
                    pie++;

                    if (fwrite != 0)
                        printf("Contents to file written successfully !\n");
                    else
                        printf("Error writing file !\n");
                    fclose(of);
                }

                //    FILE *inf;
                //  struct Filedata inp;
                //   inf = fopen("filedata.txt", "r");
                // if (inf == NULL)
                //{
                //   fprintf(stderr, "\nError to open the file\n");
                //  exit(1);
                // }
                //while (fread(&inp, sizeof(struct Filedata), 1, inf))
                //{
                //  struct stat attr;
                // printf("inode = %d modified = %s\n", inp.inodeoffile, ctime(&(inp.modifiedtime)));
                //}
                //fclose(inf);
            }
        }
    }

    printf("\npie value:%d", pie);
}
void printfile()
{
    struct Filedata inp;
    FILE *inf;
    inf = fopen("filedata2.txt", "r");
    int j = 0;
    if (inf == NULL)
    {
        fprintf(stderr, "\nError to open the file\n");
        exit(1);
    }
    while (fread(&inp, sizeof(struct Filedata), 1, inf))
    {
        struct stat attr;
        printf("inode = %d modified = %s\n", inp.inodeoffile, ctime(&(inp.modifiedtime)));
        j++;
    }
    fclose(inf);
    printf("j value:%d", j);
}
int main(void)
{

    chkfiles();

    //uploadfiles();
    printfile();
    printf("\n");
}
