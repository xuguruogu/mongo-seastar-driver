#include "hack.h"

#include <bson.h>
#include <mongoc.h>

#define MONGOC_COMPILATION
#include "private/mongoc-trace-private.h"
#include "private/mongoc-socket-private.h"
#include "private/mongoc-errno-private.h"
#include "private/mongoc-counters-private.h"
#undef MONGOC_COMPILATION

#include <core/reactor.hh>
#include <core/sleep.hh>
#include <util/defer.hh>

extern "C" {
bool _mongoc_socket_wait (mongoc_socket_t *sock, int events, int64_t expire_at);
}

static void
_mongoc_socket_capture_errno (mongoc_socket_t *sock) /* IN */
{
    sock->errno_ = errno;
    TRACE ("setting errno: %d %s", sock->errno_, strerror (sock->errno_));
}

static ssize_t
override_mongoc_socket_poll (mongoc_socket_poll_t *sds, /* IN */
                             size_t nsds,               /* IN */
                             int32_t timeout)           /* IN */
{
    if (timeout) {
        std::vector<seastar::pollable_fd> fds;
        size_t wait_event_cnt = 0;
        promise<> pro;
        future<> fut = pro.get_future();

        auto free_fds = seastar::defer([&fds] {
            for (auto& fd : fds) {
                fd.get_file_desc().set(-1);
            }
        });

        for (int i = 0; i < nsds; i++) {
            auto sd = sds + i;
            fds.emplace_back(seastar::file_desc(sd->socket->sd));

            futurize_apply([&fds, sd] {
                auto& fd = fds.back();
                return sd->events & POLLIN ? fd.readable() : fd.writeable();
            }).finally([&wait_event_cnt, &pro, &fds] {
                if (wait_event_cnt == 0) {
                    for (auto& fd : fds) {
                        fd.forget();
                    }
                }

                wait_event_cnt++;
                if (wait_event_cnt == fds.size()) {
                    pro.set_value();
                }
            });
        }

        if (timeout < 0) {
            fut.get();
        } else {
            seastar::with_timeout(
                    std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout),
                    std::move(fut)
            ).then_wrapped([] (auto&& fut) {}).get();
        }
    }

    struct pollfd *pfds;
    int ret;
    int i;

    pfds = (struct pollfd *) malloc (sizeof (*pfds) * nsds);

    for (i = 0; i < nsds; i++) {
        pfds[i].fd = sds[i].socket->sd;
        pfds[i].events = sds[i].events | POLLERR | POLLHUP;
        pfds[i].revents = 0;
    }

    ret = poll (pfds, nsds, 0);
    for (i = 0; i < nsds; i++) {
        sds[i].revents = pfds[i].revents;
    }

    free (pfds);

    return ret;
}

static bool
override__mongoc_socket_wait (mongoc_socket_t *sock, /* IN */
                              int events,            /* IN */
                              int64_t expire_at)     /* IN */
{
    if (events != POLLIN && events != POLLOUT) {
        seastar::sleep_abortable(std::chrono::milliseconds(10)).get();
        return true;
    }

    auto fd = seastar::pollable_fd(seastar::file_desc(sock->sd));
    auto free_fd = seastar::defer([&fd] {
        fd.forget();
        fd.get_file_desc().set(-1);
    });

    int timeout;
    if (expire_at < 0) {
        timeout = -1;
    } else if (expire_at == 0) {
        timeout = 0;
    } else {
        int64_t now = bson_get_monotonic_time();
        timeout = (int) (expire_at - now);
        if (timeout < 0) {
            timeout = 0;
        }
    }

    if (timeout < 0) {
        if (events & POLLIN) {
            fd.readable().get();
            return true;
        }
        if (events & POLLOUT) {
            fd.writeable().get();
            return true;
        }
        abort();
    } else if (timeout == 0) {
        struct pollfd pfd{sock->sd, (short int)(events | POLLERR | POLLHUP), 0};
        auto ret = poll(&pfd, 1, 0);

        if (ret > 0) {
            return 0 != (pfd.revents & events);
        } else if (ret < 0) {
            auto ex = std::make_exception_ptr(std::system_error(errno, std::system_category()));
            fd.abort_writer(ex);
            fd.abort_reader(ex);
            _mongoc_socket_capture_errno (sock);
            return false;
        } else {
            sock->errno_ = EAGAIN;
            return false;
        }
    } else {
        return seastar::with_timeout(
                std::chrono::steady_clock::now() + std::chrono::microseconds(timeout),
                events & POLLIN ? fd.readable() : fd.writeable()
        ).then_wrapped([&fd, &sock] (seastar::future<>&& fut) mutable {
            try {
                fut.get();
                return true;
            } catch (timed_out_error& ex) {
                mongoc_counter_streams_timeout_inc();
                sock->errno_ = ETIMEDOUT;
                return false;
            } catch (...) {
                _mongoc_socket_capture_errno (sock);
                return false;
            }
        }).get0();
    }
}

static mongoc_client_pool_t *
override_mongoc_client_pool_new (const mongoc_uri_t *uri) {
    throw std::runtime_error("mongoc pool is not allowed in mongoc seastar lib.");
}

static char prev_wait_prologue[32];
static char prev_poll_prologue[32];
static char prev_pool_prologue[32];

extern "C" {

static char prev_mongoc_topology_reconcile_prologue[32];
void mongoc_topology_reconcile (void *topology);
void override_mongoc_topology_reconcile (void *topology) {}

}

void mongoc_seastar_hack() {
    std::cout << "mongoc seastar on hack" << std::endl;
    patch_function((void *)_mongoc_socket_wait,
                   (void *)override__mongoc_socket_wait,
                   prev_wait_prologue,
                   sizeof(prev_wait_prologue));

    patch_function((void *)mongoc_socket_poll,
                   (void *)override_mongoc_socket_poll,
                   prev_poll_prologue,
                   sizeof(prev_poll_prologue));

    patch_function((void *)mongoc_client_pool_new,
                   (void *)override_mongoc_client_pool_new,
                   prev_pool_prologue,
                   sizeof(prev_pool_prologue));

    patch_function((void *)mongoc_topology_reconcile,
                   (void *)override_mongoc_topology_reconcile,
                   prev_mongoc_topology_reconcile_prologue,
                   sizeof(prev_mongoc_topology_reconcile_prologue));

    std::cout << "mongoc seastar on hack success" << std::endl;
}

void mongoc_seastar_unhack() {
    std::cout << "mongoc seastar on unhack" << std::endl;
    patch_function((void *)_mongoc_socket_wait,
                   NULL,
                   prev_wait_prologue,
                   sizeof(prev_wait_prologue));

    patch_function((void *)mongoc_socket_poll,
                   NULL,
                   prev_poll_prologue,
                   sizeof(prev_poll_prologue));

    patch_function((void *)mongoc_client_pool_new,
                   NULL,
                   prev_pool_prologue,
                   sizeof(prev_pool_prologue));

    patch_function((void *)mongoc_topology_reconcile,
                   NULL,
                   prev_mongoc_topology_reconcile_prologue,
                   sizeof(prev_mongoc_topology_reconcile_prologue));

    std::cout << "mongoc seastar on unhack success" << std::endl;
}