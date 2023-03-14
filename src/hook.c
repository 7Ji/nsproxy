#include "hook.h"

#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "lwip/priv/tcp_priv.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"

#include "direct.h"
#include "dns.h"
#include "http.h"
#include "socks.h"

/* UDP */

void udp_handle_event(void *userp, unsigned int event)
{
    struct udp_pcb *pcb = userp;
    struct sk_ops *conn = pcb->conn;
    char buffer[65535];
    ssize_t nread, nsent;
    struct pbuf *p;
    size_t i;

    if (event & (EPOLLERR | EPOLLHUP)) {
        conn->destroy(conn);
        pcb->conn = NULL;
        udp_remove(pcb);
        return;
    }

    if (event & EPOLLIN) {
        nread = conn->recv(conn, buffer, sizeof(buffer));
        if (nread > 0) {
            if ((p = pbuf_alloc_reference(buffer, nread, PBUF_REF)) == NULL) {
                fprintf(stderr, "Out of Memory.\n");
                abort();
            }
            udp_send(pcb, p);
            pbuf_free(p);
        }
    }

    if (event & EPOLLOUT) {
        for (i = 0; i < pcb->nrcvq; i++) {
            p = pcb->rcvq[i];
            if (p->len == p->tot_len) {
                nsent = pcb->conn->send(pcb->conn, p->payload, p->tot_len);
            } else {
                pbuf_copy_partial(p, buffer, p->tot_len, 0);
                nsent = pcb->conn->send(pcb->conn, buffer, p->tot_len);
            }
            if (nsent > 0) {
                pbuf_free(p);
                continue;
            } else {
                break;
            }
        }
        pcb->nrcvq -= i;
        if (pcb->nrcvq == 0) {
            conn->evctl(conn, EPOLLOUT, 0);
        } else {
            memmove(pcb->rcvq, pcb->rcvq + i,
                    pcb->nrcvq * sizeof(pcb->rcvq[0]));
            conn->evctl(conn, EPOLLOUT, 1);
        }
    }
}

static void udp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                        const ip_addr_t *addr, u16_t port)
{
    struct sk_ops *conn = pcb->conn;

    if (!p) {
        udp_remove(pcb);
        return;
    }

    if (!conn) {
        pbuf_header_force(p, (s16_t)(ip_current_header_tot_len() + UDP_HLEN));
        icmp_port_unreach(ip_current_is_v6(), p);
        pbuf_free(p);
        udp_remove(pcb);
        return;
    }

    if (pcb->nrcvq == arraysizeof(pcb->rcvq)) {
        memmove(pcb->rcvq, pcb->rcvq + 1,
                (arraysizeof(pcb->rcvq) - 1) * sizeof(pcb->rcvq[0]));
        pcb->rcvq[arraysizeof(pcb->rcvq) - 1] = p;
    } else {
        pcb->rcvq[pcb->nrcvq] = p;
        pcb->nrcvq++;
    }

    udp_handle_event(pcb, EPOLLOUT);
}

void hook_on_udp_new(struct udp_pcb *pcb)
{
    struct loopctx *loop = ip_current_netif()->state;
    struct loopconf *conf = loop_conf(loop);
    char *addr = ipaddr_ntoa(&pcb->local_ip);
    uint16_t port = pcb->local_port;

    pcb->recv = &udp_recv_cb;

    if (port == 53 && conf->dnstype != DNSHIJACK_OFF) {
        if (conf->dnstype == DNSHIJACK_DIRECT) {
            direct_udp_create(&pcb->conn, loop, &udp_handle_event, pcb);
            pcb->conn->connect(pcb->conn, addr, port);
            return;
        }
        if (conf->dnstype == DNSHIJACK_TCP) {
            tcpdns_create(&pcb->conn, loop, &udp_handle_event, pcb);
            pcb->conn->connect(pcb->conn, conf->dnssrv, port);
            return;
        }
        if (conf->dnstype == DNSHIJACK_UDP) {
            addr = conf->dnssrv;
        }
    }

    if (conf->proxytype == PROXY_SOCKS5) {
        socks_udp_create(&pcb->conn, loop, &udp_handle_event, pcb);
        pcb->conn->connect(pcb->conn, addr, port);
    }
}

/* TCP */

void tcp_handle_event(void *userp, unsigned int event)
{
    struct tcp_pcb *pcb = userp;
    struct sk_ops *conn = pcb->conn;
    ssize_t nread, nsent;

    if (event & EPOLLERR) {
        conn->destroy(conn);
        pcb->conn = NULL;
        tcp_abort(pcb);
        return;
    }

    if (event & EPOLLIN) {
        struct pbuf *p;
        size_t s = LWIP_MIN(tcp_mss(pcb), tcp_sndbuf(pcb));

        if ((p = pbuf_alloc(PBUF_RAW, s, PBUF_RAM)) == NULL) {
            fprintf(stderr, "Out of Memory.\n");
            abort();
        }

        if (!tcp_sndbuf(pcb) || tcp_sndqueuelen(pcb) > TCP_SND_QUEUELEN - 4) {
            nread = -1;
        } else {
            nread = conn->recv(conn, p->payload, s);
        }
        if (nread > 0) {
            pbuf_realloc(p, nread);
            if (tcp_write(pcb, p->payload, nread, 0) != ERR_OK) {
                fprintf(stderr, "Out of Memory.\n");
                abort();
            }
            if (pcb->sndq == NULL)
                pcb->sndq = p;
            else
                pbuf_cat(pcb->sndq, p);
            p = NULL;
            tcp_output(pcb);
        } else if (nread == 0) {
            tcp_shutdown(pcb, 0, 1);
            conn->evctl(conn, EPOLLIN, 0);
        } else if (nread == -EAGAIN) {
            conn->evctl(conn, EPOLLIN, 1);
        } else {
            conn->evctl(conn, EPOLLIN, 0);
        }

        if (p)
            pbuf_free(p);
    }

    if (event & EPOLLOUT) {
        if (pcb->rcvq == NULL) {
            nsent = -1;
        } else {
            nsent = conn->send(conn, pcb->rcvq->payload, pcb->rcvq->len);
        }
        if (nsent > 0) {
            pcb->rcvq = pbuf_free_header(pcb->rcvq, nsent);
            tcp_recved(pcb, nsent);
        } else if (nsent == -EAGAIN) {
            conn->evctl(conn, EPOLLOUT, 1);
        } else {
            conn->evctl(conn, EPOLLOUT, 0);
        }
    }

    if (event & EPOLLHUP) {
        conn->destroy(conn);
        pcb->conn = NULL;
        tcp_close(pcb);
    }
}

static err_t tcp_sent_cb(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct sk_ops *conn = pcb->conn;

    pcb->sndq = pbuf_free_header(pcb->sndq, len);

    if ((pcb->state == ESTABLISHED || pcb->state == CLOSE_WAIT) &&
        tcp_sndbuf(pcb) > TCP_SNDLOWAT)
        conn->evctl(conn, EPOLLIN, 1);

    return ERR_OK;
}

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *p,
                         err_t err)
{
    struct sk_ops *conn = pcb->conn;

    if (!conn) {
        tcp_abort(pcb);
        if (p)
            pbuf_free(p);
        return ERR_ABRT;
    }

    if (!p) {
        conn->shutdown(conn, SHUT_WR);
        return ERR_OK;
    }

    tcp_ack(pcb);

    if (pcb->rcvq)
        pbuf_cat(pcb->rcvq, p);
    else
        pcb->rcvq = p;

    tcp_handle_event(pcb, EPOLLOUT);
    return ERR_OK;
}

void hook_on_tcp_new(struct tcp_pcb *pcb)
{
    struct loopctx *loop = ip_current_netif()->state;

    tcp_nagle_disable(pcb);

    tcp_sent(pcb, &tcp_sent_cb);
    tcp_recv(pcb, &tcp_recv_cb);

    if (loop_conf(loop)->proxytype == PROXY_SOCKS5) {
        socks_tcp_create(&pcb->conn, loop, &tcp_handle_event, pcb);
    } else if (loop_conf(loop)->proxytype == PROXY_HTTP) {
        http_tcp_create(&pcb->conn, loop, &tcp_handle_event, pcb);
    } else {
        direct_tcp_create(&pcb->conn, loop, &tcp_handle_event, pcb);
    }

    pcb->conn->connect(pcb->conn, ipaddr_ntoa(&pcb->local_ip), pcb->local_port);
}