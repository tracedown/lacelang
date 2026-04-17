/*
 * mock_server — POSIX-only (Linux/macOS/WSL).
 *
 * Uses pthread + stdlib sockets. HTTP parsing is deliberately minimal:
 * reads until "\r\n\r\n", parses Content-Length, drains the body, and
 * replies.
 */

#define _POSIX_C_SOURCE 200809L

#include "mock_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

struct mock_server {
    int              listen_fd;
    int              port;
    pthread_t        thread;
    volatile int     stop_flag;
    mock_response_t *queue;
    size_t           queue_len;
    size_t           queue_pos;
    pthread_mutex_t  queue_lock;
    SSL_CTX         *tls_ctx;    /* NULL for plain HTTP listeners */
};

static const char *status_text_default(int status) {
    switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 413: return "Payload Too Large";
    case 500: return "Internal Server Error";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    default:  return "OK";
    }
}

/* In-place substitute "{port}" → decimal port in a malloc'd string. Returns
 * a new malloc'd buffer if substitution occurred (caller frees old), else
 * returns the original pointer unchanged. */
static char *substitute_port_str(char *src, int port) {
    if (!src) return NULL;
    if (!strstr(src, "{port}")) return src;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    size_t plen = strlen(portstr);
    const char *tok = "{port}";
    size_t tlen = strlen(tok);
    size_t cap = strlen(src) + 1;
    char *out = malloc(cap + 64);
    if (!out) return src;
    size_t w = 0;
    for (size_t i = 0; src[i]; ) {
        if (strncmp(src + i, tok, tlen) == 0) {
            while (w + plen + 1 >= cap) {
                cap *= 2;
                char *r = realloc(out, cap);
                if (!r) { free(out); return src; }
                out = r;
            }
            memcpy(out + w, portstr, plen);
            w += plen;
            i += tlen;
        } else {
            if (w + 1 >= cap) {
                cap *= 2;
                char *r = realloc(out, cap);
                if (!r) { free(out); return src; }
                out = r;
            }
            out[w++] = src[i++];
        }
    }
    out[w] = 0;
    free(src);
    return out;
}

/* Walk the queue and substitute "{port}" in status_text, header values,
 * and body with the bound port. Vectors author mock headers like
 * "location: http://127.0.0.1:{port}/dest"; port is ephemeral so the
 * substitution has to happen after bind. */
static void substitute_port_in_queue(mock_response_t *q, size_t n, int port) {
    for (size_t i = 0; i < n; i++) {
        q[i].status_text = substitute_port_str(q[i].status_text, port);
        for (size_t h = 0; h < q[i].header_count; h++) {
            q[i].headers[h * 2 + 1] =
                substitute_port_str(q[i].headers[h * 2 + 1], port);
        }
        if (q[i].body && q[i].body_len > 0 && strstr(q[i].body, "{port}")) {
            char *sub = substitute_port_str(q[i].body, port);
            if (sub != q[i].body) {
                q[i].body = sub;
                q[i].body_len = strlen(sub);
            }
        }
    }
}

/* Read bytes until we see "\r\n\r\n" or the peer closes. Returns bytes read
 * into `buf`, or -1 on error. Leaves a NUL terminator. */
static ssize_t read_headers(int fd, char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        ssize_t r = recv(fd, buf + n, cap - 1 - n, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;  /* peer closed */
        n += (size_t)r;
        buf[n] = 0;
        if (strstr(buf, "\r\n\r\n")) return (ssize_t)n;
    }
    buf[n] = 0;
    return (ssize_t)n;
}

static long parse_content_length(const char *headers) {
    const char *p = headers;
    while ((p = strcasestr(p, "content-length:"))) {
        /* ensure we're at start-of-line */
        if (p == headers || *(p - 1) == '\n') {
            p += strlen("content-length:");
            while (*p == ' ' || *p == '\t') p++;
            return strtol(p, NULL, 10);
        }
        p++;
    }
    return 0;
}

/* Drain `n` bytes from the socket into a throwaway buffer. */
static int drain_body(int fd, long n) {
    char chunk[4096];
    while (n > 0) {
        ssize_t got = recv(fd, chunk, (n > (long)sizeof(chunk)) ? sizeof(chunk) : (size_t)n, 0);
        if (got <= 0) return -1;
        n -= got;
    }
    return 0;
}

static int send_all(int fd, const char *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        ssize_t w = send(fd, buf + sent, n - sent, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (w == 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}

static void serve_response(int client_fd, const mock_response_t *resp) {
    char head[8192];
    int off = snprintf(head, sizeof(head),
                       "HTTP/1.1 %d %s\r\n",
                       resp->status,
                       resp->status_text ? resp->status_text
                                         : status_text_default(resp->status));
    bool has_content_length = false;
    for (size_t i = 0; i < resp->header_count; i++) {
        const char *name  = resp->headers[i * 2];
        const char *value = resp->headers[i * 2 + 1];
        if (!name || !value) continue;
        if (strcasecmp(name, "content-length") == 0) has_content_length = true;
        off += snprintf(head + off, sizeof(head) - off, "%s: %s\r\n", name, value);
    }
    if (!has_content_length) {
        off += snprintf(head + off, sizeof(head) - off,
                        "Content-Length: %zu\r\n", resp->body_len);
    }
    off += snprintf(head + off, sizeof(head) - off, "Connection: close\r\n\r\n");
    if (send_all(client_fd, head, (size_t)off) != 0) return;
    if (resp->body && resp->body_len > 0) {
        send_all(client_fd, resp->body, resp->body_len);
    }
}

/* ── TLS handling ─────────────────────────────────────────────────── */

/* One-time global OpenSSL init. OpenSSL 1.1+ initialises implicitly; this
 * call just guarantees error strings are loaded so SSL_accept failures
 * surface with readable reasons. */
static void tls_init_callback(void) {
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS
                     | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
}
static void tls_init_once(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, tls_init_callback);
}

static SSL_CTX *build_tls_ctx(const char *cert_path, const char *key_path) {
    tls_init_once();
    const SSL_METHOD *m = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(m);
    if (!ctx) return NULL;
    /* Lock to TLS 1.2+; no legacy protocols in tests. */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path) != 1) {
        fprintf(stderr, "mock_server: load cert %s failed: ", cert_path);
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "mock_server: load key %s failed: ", key_path);
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
    if (SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "mock_server: cert and key do not match (%s / %s)\n", cert_path, key_path);
        SSL_CTX_free(ctx);
        return NULL;
    }
    return ctx;
}

/* Buffered send/recv that speak through the SSL session. */
static ssize_t tls_recv(SSL *ssl, void *buf, size_t n) {
    int r = SSL_read(ssl, buf, (int)n);
    return r > 0 ? (ssize_t)r : -1;
}

static int tls_send_all(SSL *ssl, const char *buf, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        int w = SSL_write(ssl, buf + sent, (int)(n - sent));
        if (w <= 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}

/* Same handler pattern as the plain path but reads/writes via SSL. */
static void handle_client_tls(mock_server_t *s, int client_fd) {
    SSL *ssl = SSL_new(s->tls_ctx);
    if (!ssl) { close(client_fd); return; }
    SSL_set_fd(ssl, client_fd);

    if (SSL_accept(ssl) != 1) {
        /* Handshake failed — client probably rejected the cert. Close cleanly. */
        SSL_free(ssl);
        close(client_fd);
        return;
    }

    char buf[16384];
    size_t total = 0;
    ssize_t r;
    while (total + 1 < sizeof(buf) && (r = tls_recv(ssl, buf + total, sizeof(buf) - 1 - total)) > 0) {
        total += (size_t)r;
        buf[total] = 0;
        if (strstr(buf, "\r\n\r\n")) break;
    }

    long cl = parse_content_length(buf);
    char *header_end = strstr(buf, "\r\n\r\n");
    size_t already = header_end ? (size_t)(total - (size_t)(header_end + 4 - buf)) : 0;
    while (cl > 0 && already < (size_t)cl) {
        char sink[4096];
        int want = (cl - (long)already > (long)sizeof(sink)) ? (int)sizeof(sink) : (int)(cl - (long)already);
        int got = SSL_read(ssl, sink, want);
        if (got <= 0) break;
        already += (size_t)got;
    }

    pthread_mutex_lock(&s->queue_lock);
    mock_response_t *resp = NULL;
    if (s->queue_pos < s->queue_len) {
        resp = &s->queue[s->queue_pos];
        if (!resp->sticky) s->queue_pos++;
    }
    pthread_mutex_unlock(&s->queue_lock);

    if (!resp) {
        static const char fallback[] =
            "HTTP/1.1 500 No Mock Response\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        tls_send_all(ssl, fallback, sizeof(fallback) - 1);
        goto done;
    }
    if (resp->outcome == MOCK_OUTCOME_TIMEOUT) {
        for (int i = 0; i < 200; i++) {
            if (s->stop_flag) break;
            struct timespec ts = { 0, 100 * 1000 * 1000 };
            nanosleep(&ts, NULL);
        }
        goto done;
    }
    if (resp->delay_ms > 0) {
        struct timespec ts = {
            .tv_sec  = resp->delay_ms / 1000,
            .tv_nsec = (resp->delay_ms % 1000) * 1000 * 1000L,
        };
        nanosleep(&ts, NULL);
    }

    char head[8192];
    int off = snprintf(head, sizeof(head), "HTTP/1.1 %d %s\r\n",
                       resp->status,
                       resp->status_text ? resp->status_text : status_text_default(resp->status));
    int has_cl = 0;
    for (size_t i = 0; i < resp->header_count; i++) {
        const char *name  = resp->headers[i * 2];
        const char *value = resp->headers[i * 2 + 1];
        if (!name || !value) continue;
        if (strcasecmp(name, "content-length") == 0) has_cl = 1;
        off += snprintf(head + off, sizeof(head) - off, "%s: %s\r\n", name, value);
    }
    if (!has_cl)
        off += snprintf(head + off, sizeof(head) - off, "Content-Length: %zu\r\n", resp->body_len);
    off += snprintf(head + off, sizeof(head) - off, "Connection: close\r\n\r\n");
    tls_send_all(ssl, head, (size_t)off);
    if (resp->body && resp->body_len > 0) tls_send_all(ssl, resp->body, resp->body_len);

done:
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_fd);
}

static void handle_client(mock_server_t *s, int client_fd) {
    struct timeval to = { .tv_sec = 5, .tv_usec = 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to));

    char buf[16384];
    ssize_t n = read_headers(client_fd, buf, sizeof(buf));
    if (n <= 0) { close(client_fd); return; }

    long cl = parse_content_length(buf);
    char *header_end = strstr(buf, "\r\n\r\n");
    size_t already_read_body = 0;
    if (header_end) {
        header_end += 4;
        already_read_body = (size_t)(n - (header_end - buf));
    }
    if (cl > 0 && already_read_body < (size_t)cl) {
        drain_body(client_fd, cl - (long)already_read_body);
    }

    pthread_mutex_lock(&s->queue_lock);
    mock_response_t *resp = NULL;
    if (s->queue_pos < s->queue_len) {
        resp = &s->queue[s->queue_pos];
        if (!resp->sticky) s->queue_pos++;
    }
    pthread_mutex_unlock(&s->queue_lock);

    if (!resp) {
        /* out of programmed responses — 500 */
        static const char fallback[] =
            "HTTP/1.1 500 No Mock Response\r\n"
            "Content-Length: 0\r\n"
            "Connection: close\r\n\r\n";
        send_all(client_fd, fallback, sizeof(fallback) - 1);
        close(client_fd);
        return;
    }

    if (resp->outcome == MOCK_OUTCOME_TIMEOUT) {
        /* Hold the connection open — let the executor time out. */
        /* Sleep up to ~20s unless server is being stopped. */
        for (int i = 0; i < 200; i++) {
            if (s->stop_flag) break;
            struct timespec ts = { 0, 100 * 1000 * 1000 };  /* 100 ms */
            nanosleep(&ts, NULL);
        }
        close(client_fd);
        return;
    }

    if (resp->delay_ms > 0) {
        struct timespec ts = {
            .tv_sec  = resp->delay_ms / 1000,
            .tv_nsec = (resp->delay_ms % 1000) * 1000 * 1000L,
        };
        nanosleep(&ts, NULL);
    }

    serve_response(client_fd, resp);
    close(client_fd);
}

static void *server_thread(void *arg) {
    mock_server_t *s = (mock_server_t *)arg;
    while (!s->stop_flag) {
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        /* poll with small timeout so stop_flag is noticed. */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s->listen_fd, &rfds);
        struct timeval tv = { .tv_sec = 0, .tv_usec = 100 * 1000 };
        int r = select(s->listen_fd + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) continue;
        int client_fd = accept(s->listen_fd, (struct sockaddr *)&peer, &peer_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (s->tls_ctx) { handle_client_tls(s, client_fd); continue; }
        handle_client(s, client_fd);
    }
    return NULL;
}

mock_server_t *mock_server_start_tls(
    mock_response_t *responses, size_t count,
    const char *cert_path, const char *key_path
) {
    mock_server_t *s = mock_server_start(responses, count);
    if (!s) return NULL;
    s->tls_ctx = build_tls_ctx(cert_path, key_path);
    if (!s->tls_ctx) {
        mock_server_stop(s);
        return NULL;
    }
    return s;
}

mock_server_t *mock_server_start(mock_response_t *responses, size_t count) {
    mock_server_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->queue = responses;
    s->queue_len = count;
    pthread_mutex_init(&s->queue_lock, NULL);

    s->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->listen_fd < 0) { free(s); return NULL; }

    int one = 1;
    setsockopt(s->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(s->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(s->listen_fd); free(s); return NULL;
    }
    if (listen(s->listen_fd, 16) < 0) {
        close(s->listen_fd); free(s); return NULL;
    }
    socklen_t len = sizeof(addr);
    if (getsockname(s->listen_fd, (struct sockaddr *)&addr, &len) < 0) {
        close(s->listen_fd); free(s); return NULL;
    }
    s->port = ntohs(addr.sin_port);

    /* Substitute {port} placeholders in the queue now that the ephemeral
     * port is known. Supports vectors whose mock responses reference the
     * bound port (e.g. absolute redirect Location headers). */
    if (s->queue && s->queue_len > 0) {
        substitute_port_in_queue(s->queue, s->queue_len, s->port);
    }

    if (pthread_create(&s->thread, NULL, server_thread, s) != 0) {
        close(s->listen_fd); free(s); return NULL;
    }
    return s;
}

int mock_server_port(const mock_server_t *s) { return s->port; }

static void free_response(mock_response_t *r) {
    free(r->status_text);
    if (r->headers) {
        for (size_t i = 0; i < r->header_count * 2; i++) free(r->headers[i]);
        free(r->headers);
    }
    free(r->body);
}

void mock_server_stop(mock_server_t *s) {
    if (!s) return;
    s->stop_flag = 1;
    pthread_join(s->thread, NULL);
    close(s->listen_fd);
    pthread_mutex_destroy(&s->queue_lock);
    for (size_t i = 0; i < s->queue_len; i++) free_response(&s->queue[i]);
    free(s->queue);
    if (s->tls_ctx) SSL_CTX_free(s->tls_ctx);
    free(s);
}
