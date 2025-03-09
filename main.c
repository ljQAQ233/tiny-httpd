#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
        
char buf[4096];

#define MAX_PATH 128

struct request
{
    char *buf;
    char method[8];
    char path[MAX_PATH];
    char qury[MAX_PATH];
    char proto[16];
    char host[128];
    char agent[64];
};

int sr; // socket
int cu; // accept

char *errmsg[600] = {
    [200] = "OK",
    [301] = "Moved Permanently",
    [400] = "Bad Request",
    [403] = "Forbidden",
    [404] = "Not Found",
    [414] = "URI Too Long",
    [500] = "Internal Server Error",
    [501] = "Not Implemented",
    [503] = "Service Unavailable",
};

#define CRLF "\r\n"

int tiny_rp(int err, char *type, int len, void *body)
{
    char hdr[256];

    if (!errmsg[err])
        return -1;

    dprintf(cu, "HTTP/1.1 %d %s" CRLF, err, errmsg[err]);
    dprintf(cu, "Content-type: %s" CRLF, type);
    dprintf(cu, "Content-length: %d" CRLF, len);
    dprintf(cu, CRLF);
    return write(cu, body, len) & 0;
}

char *tiny_ver = "tiny/1.0";
char tiny_root[MAX_PATH]; // site root path

int tiny_err(int err)
{
    char html[128];
    int len = sprintf(html,
            "<html>\n"
            "<title>Tiny Error</title>\n"
            "<body>\n"
            "<center><h1>%d %s</h1></center>\n"
            "<hr><center>%s</center>\n"
            "</body>\n"
            "</html>\n",
            err, errmsg[err], tiny_ver);
    return tiny_rp(err, "text/html", len, html);
}

int xmeth(struct request *r)
{
    if (strcmp(r->method, "GET") == 0)
        return 0;

    return 1;
}

int xpath(struct request *r)
{
    char *p = r->path;
    char *q = r->path;

    char *qry = NULL;

    while (*p)
    {
        if (*p == '+')
        {
            *q = ' ';
            p += 1;
        }
        else if (*p == '%')
        {
            char hex[3];
            hex[0] = *(p + 1);
            hex[1] = *(p + 2);
            hex[2] = 0;
            *q = (char)strtol(hex, NULL, 16);
            p += 1 + 2;
        }
        else if (*p == '?')
        {
            *q = 0;
            qry = q+1;
            p += 1;
        }
        else
        {
            *q = *p;
            p += 1;
        }
        q += 1;
    }
    *q = 0;

    if (qry)
    {
        strncpy(r->qury, qry, sizeof(r->qury));
    }

    return 0;
}

int parse(char *buf, struct request *r)
{
    r->buf = buf;
    sscanf(buf, "%s %s %s",
            r->method,
            r->path,
            r->proto);

    xmeth(r); // test if method is supported
    xpath(r); // get path separated
    return 0;
}

int acces(char *real, char *path)
{
    char tmp[MAX_PATH];
    sprintf(tmp, "site" "%s", path);
    realpath(tmp, real);
    if (strncmp(real, tiny_root, strlen(tiny_root)))
        return -1;

    return 0;
}

char *stype(char *file)
{
    char *type = "text/plain";

    if (strstr(file, ".html"))
        type = "text/html";
    else if (strstr(file, ".css"))
        type = "text/css";

    else if (strstr(file, ".gif"))
        type = "image/gif";
    else if (strstr(file, ".png"))
        type = "image/png";
    else if (strstr(file, ".jpg"))
        type = "image/jpeg";

    else if (strstr(file, ".js"))
        type = "application/javascript";
    else if (strstr(file, ".json"))
        type = "application/json";
    else if (strstr(file, ".xml"))
        type = "application/xml";

    else if (strstr(file, ".mp4"))
        type = "video/mp4";
    else if (strstr(file, ".webm"))
        type = "video/webm";
    else if (strstr(file, ".mov"))
        type = "video/quicktime";

    else if (strstr(file, ".mp3"))
        type = "audio/mp3";

    return type;
}

int serve(struct request *r)
{
    // static files
    char path[MAX_PATH];
    if (acces(path, r->path) < 0)
    {
        tiny_err(403);
        return -1;
    }

    struct stat sb;
    if (stat(path, &sb) < 0)
    {
        tiny_err(404);
        return -1;
    }

    printf("GET %s\n", path);

    int len = sb.st_size;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        tiny_err(404);
        return -1;
    }

    void *buf = mmap(
            NULL, len,
            PROT_READ, MAP_PRIVATE,
            fd, 0);
    char *type = stype(path);
    tiny_rp(200, type, len, buf);

    munmap(buf, len);
    close(fd);
    return 0;
}

int tiny_main(int port)
{
    sr = socket(AF_INET, SOCK_STREAM, 0);
    if (sr < 0)
        goto die;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr = INADDR_ANY,
        .sin_port = htons(port),
    };

    if (bind(sr, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        goto die;

    if (listen(sr, 1) < 0)
        goto die;

    for (;;)
    {
        cu = accept(sr, NULL, NULL);
        read(cu, buf, sizeof(buf));

        struct request r;
        parse(buf, &r);
        serve(&r);
        close(cu);
    }

die:
    if (sr > 0)
        close(sr);
    return 0;
}

int main(int argc, char *argv[])
{
    getcwd(tiny_root, sizeof(tiny_root));
    strcat(tiny_root, "/site");

    if (argc != 2)
        return tiny_main(8082);

    return tiny_main(atoi(argv[1]));
}
