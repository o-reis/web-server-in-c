#ifndef HTTP_H
#define HTTP_H
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct
{
    char method[16];
    char path[512];
    char version[16];
    int connection_close;
} http_request_t;

int parse_http_request(const char *buffer, http_request_t *req);
void send_http_response(SSL *ssl, int status, const char *status_msg, const char *content_type, const char *body, size_t body_len, int keep_alive);
char *read_http(char *pathBuffer, size_t *out_size);

#endif