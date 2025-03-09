#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
        
char buf[4096];

struct request
{
    char *buf;
    char method[8];
    char path[128];
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
    return 0;
}

int parse(char *buf, struct request *r)
{
    r->buf = buf;
    sscanf(buf, "%s %s %s",
            r->method,
            r->path,
            r->proto);
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
        printf("recv...\n");
        cu = accept(sr, NULL, NULL);
        read(cu, buf, sizeof(buf));

        tiny_err(200);

        close(cu);
    }

die:
    if (sr > 0)
        close(sr);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        return 1;

    return tiny_main(atoi(argv[1]));
}
