/*
 * mock_server — minimal threaded HTTP/1.1 server used by execute and
 * extension vector runners.
 *
 * A server instance binds 127.0.0.1:0 (ephemeral port) and serves a queue
 * of pre-programmed responses in the order the executor issues requests.
 * The server thread handles a bounded number of requests then exits.
 *
 * Not an HTTP server in the general-purpose sense — understands only the
 * request line + headers, reads Content-Length (or chunked) request bodies,
 * and emits whatever response is at the head of the queue.
 */

#ifndef MOCK_SERVER_H
#define MOCK_SERVER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MOCK_OUTCOME_RESPONSE,   /* reply with the programmed status/headers/body */
    MOCK_OUTCOME_TIMEOUT,    /* accept the connection but never reply         */
} mock_outcome_t;

typedef struct {
    mock_outcome_t outcome;
    int            status;
    char          *status_text;        /* owned — may be NULL for default  */
    /* Response headers: flat NULL-terminated array of "name" / "value" pairs */
    char         **headers;            /* owned; each string owned         */
    size_t         header_count;       /* number of name/value *pairs*     */
    char          *body;               /* owned; may be NULL for no body   */
    size_t         body_len;
    /* Optional sticky behaviour. When non-zero, the same response serves
     * subsequent requests too (queue position does not advance). Used by
     * `redirect_to` semantics — a single entry describes a redirect that
     * applies to every hop until the executor's redirect limit fires. */
    int            sticky;
    /* Optional millisecond delay before sending the response — used to
     * model slow first-byte (`ttfb_delay_ms`) or transfer slowness. */
    int            delay_ms;
} mock_response_t;

typedef struct mock_server mock_server_t;

/* Create a server instance, bind to 127.0.0.1:0, begin accepting.
 * Takes ownership of `responses` (will free each + their fields on stop).
 * Returns NULL on failure. */
mock_server_t *mock_server_start(mock_response_t *responses, size_t count);

/* Same, but listen for TLS. `cert_path` / `key_path` must be PEM files on
 * disk. Returns NULL if the cert/key could not be loaded. */
mock_server_t *mock_server_start_tls(
    mock_response_t *responses,
    size_t           count,
    const char      *cert_path,
    const char      *key_path
);

/* The port the server is listening on. */
int mock_server_port(const mock_server_t *s);

/* Signal the server thread to stop, join, and free all owned resources. */
void mock_server_stop(mock_server_t *s);

#endif
