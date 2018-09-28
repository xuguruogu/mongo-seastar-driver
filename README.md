Hack mongo-c-driver to make it usable in seastar under async context. And thus we can use mongo-c-driver and mongo-cxx-driver.

Before use it, you should modify file mongoc-socket.c of mongo-c-driver

```

static bool
_mongoc_socket_wait (mongoc_socket_t *sock, /* IN */
                     int events,            /* IN */
                     int64_t expire_at)     /* IN */
```

to

```
bool
_mongoc_socket_wait (mongoc_socket_t *sock, /* IN */
                     int events,            /* IN */
                     int64_t expire_at)     /* IN */
```

