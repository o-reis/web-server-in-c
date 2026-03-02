#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <strings.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "http.h"

// Parses an HTTP request string and extracts method, path, and headers
int parse_http_request(const char *buffer, http_request_t *req)
{
    req->connection_close = 0;

    char *line_end = strstr(buffer, "\r\n");
    if (!line_end)
        return -1;
    char first_line[1024];
    size_t len = line_end - buffer;
    strncpy(first_line, buffer, len);
    first_line[len] = '\0';
    if (sscanf(first_line, "%s %s %s", req->method, req->path, req->version) != 3)
    {
        return -1;
    }

    const char *conn_header = strcasestr(buffer, "Connection:");
    if (conn_header)
    {
        if (strcasestr(conn_header, "close"))
        {
            req->connection_close = 1;
        }
    }

    return 0;
}

// Sends an HTTP response with headers and body over SSL connection
void send_http_response(SSL *ssl, int status, const char *status_msg, const char *content_type, const char *body, size_t body_len, int keep_alive)
{
    char header[2048];

    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Server: ConcurrentHTTP/1.0\r\n"
                              "Connection: %s\r\n"
                              "\r\n",
                              status, status_msg, content_type, body_len, keep_alive ? "keep-alive" : "close");
    SSL_write(ssl, header, header_len);
    if (body && body_len > 0)
    {
        SSL_write(ssl, body, body_len);
    }
}

// Reads a file from disk and returns its contents as a buffer
char *read_http(char *pathBuffer, size_t *out_size)
{
    FILE *fp = fopen(pathBuffer, "rb"); // Open in binary mode
    if (!fp)
    {
        if (out_size)
            *out_size = 0;
        char *empty = malloc(1);
        empty[0] = '\0';
        return empty;
    }

    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);

    char *buf = malloc(size + 1);
    if (!buf)
    {
        fclose(fp);
        if (out_size)
            *out_size = 0;
        char *empty = malloc(1);
        empty[0] = '\0';
        return empty;
    }

    size_t read_size = fread(buf, 1, size, fp);
    buf[read_size] = '\0';

    fclose(fp);

    if (out_size)
        *out_size = read_size;
    return buf;
}