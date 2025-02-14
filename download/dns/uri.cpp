
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uri.h"


namespace uri {
    static void ast_copy_string(char *dst, const char *src, size_t size) {
        while (*src && size) {
            *dst++ = *src++;
            size--;
        }
        if (__builtin_expect(!size, 0))
            dst--;
        *dst = '\0';
    }

    static int ast_strlen_zero(const char *s) {
        return (!s || (*s == '\0'));
    }


/*!
 * \brief Construct a uri object with the given values.
 *
 * \note The size parameters [should] include room for the string terminator
 *       (strlen(<param>) + 1). For instance, if a scheme of 'http' is given
 *       then the 'scheme_size' should be equal to 5.
 */
    static struct ast_uri *ast_uri_create_(
            const char *scheme, unsigned int scheme_size,
            const char *user_info, unsigned int user_info_size,
            const char *host, unsigned int host_size,
            const char *port, unsigned int port_size,
            const char *path, unsigned int path_size,
            const char *query, unsigned int query_size) {
#define SET_VALUE(param, field, size) \
    do { if (param) { \
         ast_copy_string(p, param, size); \
         field = p; \
         p += size;    } } while (0)

        char *p;
        struct ast_uri *res = (struct ast_uri *) calloc(1,
                                                        sizeof(*res) + scheme_size + user_info_size + host_size +
                                                        port_size + path_size + query_size);

        if (!res) {
            printf("Unable to create URI object\n");
            return NULL;
        }

        p = res->uri;
        SET_VALUE(scheme, res->scheme, scheme_size);
        SET_VALUE(user_info, res->user_info, user_info_size);
        SET_VALUE(host, res->host, host_size);
        SET_VALUE(port, res->port, port_size);
        SET_VALUE(path, res->path, path_size);
        SET_VALUE(query, res->query, query_size);
        return res;
    }

    struct ast_uri *ast_uri_create(const char *scheme, const char *user_info,
                                   const char *host, const char *port,
                                   const char *path, const char *query) {
        return ast_uri_create_(
                scheme, scheme ? strlen(scheme) + 1 : 0,
                user_info, user_info ? strlen(user_info) + 1 : 0,
                host, host ? strlen(host) + 1 : 0,
                port, port ? strlen(port) + 1 : 0,
                path, path ? strlen(path) + 1 : 0,
                query, query ? strlen(query) + 1 : 0);
    }

    struct ast_uri *ast_uri_copy_replace(const struct ast_uri *uri, const char *scheme,
                                         const char *user_info, const char *host,
                                         const char *port, const char *path,
                                         const char *query) {
        return ast_uri_create(
                scheme ? scheme : uri->scheme,
                user_info ? user_info : uri->user_info,
                host ? host : uri->host,
                port ? port : uri->port,
                path ? path : uri->path,
                query ? query : uri->query);
    }

    const char *ast_uri_scheme(const struct ast_uri *uri) {
        return uri->scheme;
    }

    const char *ast_uri_user_info(const struct ast_uri *uri) {
        return uri->user_info;
    }

    const char *ast_uri_host(const struct ast_uri *uri) {
        return uri->host;
    }

    const char *ast_uri_port(const struct ast_uri *uri) {
        return uri->port;
    }

    const char *ast_uri_path(const struct ast_uri *uri) {
        return uri->path;
    }

    const char *ast_uri_query(const struct ast_uri *uri) {
        return uri->query;
    }

    int ast_uri_is_secure(const struct ast_uri *uri) {
        return ast_strlen_zero(uri->scheme) ? 0 :
               *(uri->scheme + strlen(uri->scheme) - 1) == 's';
    }

    struct ast_uri *ast_uri_parse(const char *uri) {
#define SET_VALUES(value) \
    value = uri; \
    size_##value = p - uri + 1; \
    uri = p + 1;

        const char *p, *scheme = NULL, *user_info = NULL, *host = NULL;
        const char *port = NULL, *path = NULL, *query = NULL;
        unsigned int size_scheme = 0, size_user_info = 0, size_host = 0;
        unsigned int size_port = 0, size_path = 0, size_query = 0;

        if ((p = strstr(uri, "://"))) {
            scheme = uri;
            size_scheme = p - uri + 1;
            uri = p + 3;
        }

        if ((p = strchr(uri, '@'))) {
            SET_VALUES(user_info);
        }

        if ((p = strchr(uri, ':'))) {
            SET_VALUES(host);
        }

        if ((p = strchr(uri, '/'))) {
            if (!host) {
                SET_VALUES(host);
            } else {
                SET_VALUES(port);
            }
        }

        if ((p = strchr(uri, '?'))) {
            query = p + 1;
            size_query = strlen(query) + 1;
        } else {
            p = uri + strlen(uri);
        }

        if (!host) {
            SET_VALUES(host);
        } else if (*(uri - 1) == ':') {
            SET_VALUES(port);
        } else if (*(uri - 1) == '/') {
            SET_VALUES(path);
        }

        return ast_uri_create_(scheme, size_scheme,
                               user_info, size_user_info,
                               host, size_host,
                               port, size_port,
                               path, size_path,
                               query, size_query);
    }

    static struct ast_uri *uri_parse_and_default(const char *uri, const char *scheme,
                                                 const char *port, const char *secure_port) {
        struct ast_uri *res;
        int len = strlen(scheme);

        if (!strncmp(uri, scheme, len)) {
            res = ast_uri_parse(uri);
        } else {
            /* make room for <scheme>:// */
            char *with_scheme = (char *) calloc(1, len + strlen(uri) + 4);
            if (!with_scheme) {
                printf("Unable to allocate uri '%s' with "
                       "scheme '%s'", uri, scheme);
                return NULL;
            }

            /* safe - 'with_scheme' created with size equal to len of
               scheme plus length of uri plus space for extra characters
               '://' and terminator */
            sprintf(with_scheme, "%s://%s", scheme, uri);
            res = ast_uri_parse(with_scheme);
            free(with_scheme);
        }

        if (res && ast_strlen_zero(ast_uri_port(res))) {
            /* default the port if not given */
            struct ast_uri *tmp = ast_uri_copy_replace(
                    res, NULL, NULL, NULL,
                    ast_uri_is_secure(res) ? secure_port : port,
                    NULL, NULL);
            free(res);
            res = tmp;
        }
        return res;
    }

    struct ast_uri *ast_uri_parse_http(const char *uri) {
        return uri_parse_and_default(uri, "http", "80", "443");
    }

    struct ast_uri *ast_uri_parse_websocket(const char *uri) {
        return uri_parse_and_default(uri, "ws", "80", "443");
    }

    char *ast_uri_make_host_with_port(const struct ast_uri *uri) {
        int host_size = ast_uri_host(uri) ?
                        strlen(ast_uri_host(uri)) : 0;
        /* if there is a port +1 for the colon */
        int port_size = ast_uri_port(uri) ?
                        strlen(ast_uri_port(uri)) + 1 : 0;
        char *res = (char *) calloc(1, host_size + port_size + 1);

        if (!res) {
            return NULL;
        }

        memcpy(res, ast_uri_host(uri), host_size);

        if (ast_uri_port(uri)) {
            res[host_size] = ':';
            memcpy(res + host_size + 1,
                   ast_uri_port(uri), port_size - 1);
        }

        res[host_size + port_size] = '\0';
        return res;
    }

}
