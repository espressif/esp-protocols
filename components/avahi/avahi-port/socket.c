/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dns.h"
#include "log.h"
#include "addr-util.h"
#include "assert.h"
#include "avahi-core/socket.h"
#include <sys/ioctl.h>  // Add this for ioctl

static void mdns_mcast_group_ipv4(struct sockaddr_in *ret_sa)
{
    assert(ret_sa);
    memset(ret_sa, 0, sizeof(struct sockaddr_in));
    ret_sa->sin_family = AF_INET;
    ret_sa->sin_port = htons(AVAHI_MDNS_PORT);
    inet_pton(AF_INET, AVAHI_IPV4_MCAST_GROUP, &ret_sa->sin_addr);
}

static void ipv4_address_to_sockaddr(struct sockaddr_in *ret_sa, const AvahiIPv4Address *a, uint16_t port)
{
    assert(ret_sa);
    assert(a);
    assert(port > 0);

    memset(ret_sa, 0, sizeof(struct sockaddr_in));
    ret_sa->sin_family = AF_INET;
    ret_sa->sin_port = htons(port);
    memcpy(&ret_sa->sin_addr, a, sizeof(AvahiIPv4Address));
}

int avahi_mdns_mcast_join_ipv4(int fd, const AvahiIPv4Address *a, int idx, int join)
{
    struct ip_mreq mreq;
    struct sockaddr_in sa;

    assert(fd >= 0);
    assert(idx >= 0);
    assert(a);

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_interface.s_addr = a->address;
    mdns_mcast_group_ipv4(&sa);
    mreq.imr_multiaddr = sa.sin_addr;

    if (setsockopt(fd, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        avahi_log_warn("%s failed: %s", join ? "IP_ADD_MEMBERSHIP" : "IP_DROP_MEMBERSHIP", strerror(errno));
        return -1;
    }

    return 0;
}

int avahi_open_socket_ipv4(int no_reuse)
{
    struct sockaddr_in local;
    int fd = -1;
    int yes = 1;
    uint8_t ttl = 255;

    // Create socket
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        avahi_log_warn("socket() failed: %s", strerror(errno));
        goto fail;
    }

    // Set multicast TTL
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
        avahi_log_warn("IP_MULTICAST_TTL failed: %s", strerror(errno));
        goto fail;
    }

    // Enable multicast loopback
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("IP_MULTICAST_LOOP failed: %s", strerror(errno));
        goto fail;
    }

    // Set SO_REUSEADDR
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        avahi_log_warn("SO_REUSEADDR failed: %s", strerror(errno));
        goto fail;
    }

    // Bind to port
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(AVAHI_MDNS_PORT);
    local.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        avahi_log_warn("bind() failed: %s", strerror(errno));
        goto fail;
    }

    return fd;

fail:
    if (fd >= 0) {
        close(fd);
    }
    return -1;
}

int avahi_send_dns_packet_ipv4(
    int fd,
    AvahiIfIndex interface,
    AvahiDnsPacket *p,
    const AvahiIPv4Address *src_address,
    const AvahiIPv4Address *dst_address,
    uint16_t dst_port)
{

    struct sockaddr_in sa;
    ssize_t r;

    assert(fd >= 0);
    assert(p);
    assert(avahi_dns_packet_check_valid(p) >= 0);
    assert(!dst_address || dst_port > 0);

    if (!dst_address) {
        mdns_mcast_group_ipv4(&sa);
    } else {
        ipv4_address_to_sockaddr(&sa, dst_address, dst_port);
    }

    r = sendto(fd, AVAHI_DNS_PACKET_DATA(p), p->size, 0, (struct sockaddr *)&sa, sizeof(sa));

    if (r < 0) {
        avahi_log_warn("sendto() failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

AvahiDnsPacket *avahi_recv_dns_packet_ipv4(
    int fd,
    AvahiIPv4Address *ret_src_address,
    uint16_t *ret_src_port,
    AvahiIPv4Address *ret_dst_address,
    AvahiIfIndex *ret_iface,
    uint8_t *ret_ttl)
{

    AvahiDnsPacket *p = NULL;
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    ssize_t length;

    assert(fd >= 0);

    // Use fixed buffer size for ESP32 (typical MTU is 1500)
    const int ESP32_MTU = 1500;
    p = avahi_dns_packet_new(ESP32_MTU + AVAHI_DNS_PACKET_EXTRA_SIZE);
    if (!p) {
        avahi_log_warn("Failed to allocate packet buffer");
        goto fail;
    }

    length = recvfrom(fd, AVAHI_DNS_PACKET_DATA(p), p->max_size, 0,
                      (struct sockaddr *)&src_addr, &src_addr_len);

    if (length < 0) {
        if (errno != EAGAIN) {
            avahi_log_warn("recvfrom() failed: %s", strerror(errno));
        }
        goto fail;
    }

    p->size = (size_t)length;

    if (ret_src_port) {
        *ret_src_port = ntohs(src_addr.sin_port);
    }

    if (ret_src_address) {
        memcpy(&ret_src_address->address, &src_addr.sin_addr, sizeof(struct in_addr));
    }

    if (ret_ttl) {
        *ret_ttl = 255;
    }

    if (ret_iface) {
        *ret_iface = 1;    // Default to first interface for ESP32
    }

    return p;

fail:
    if (p) {
        avahi_dns_packet_free(p);
    }
    return NULL;
}

// Stub functions for IPv6 and unicast - not implemented for ESP32
int avahi_open_socket_ipv6(int no_reuse)
{
    return -1;
}

int avahi_open_unicast_socket_ipv4(void)
{
    return -1;
}

int avahi_open_unicast_socket_ipv6(void)
{
    return -1;
}


// Dummy IPv6 implementations
int avahi_mdns_mcast_join_ipv6(int fd, const AvahiIPv6Address *a, int idx, int join)
{
    return -1;  // Not supported on ESP32
}

int avahi_send_dns_packet_ipv6(
    int fd,
    AvahiIfIndex interface,
    AvahiDnsPacket *p,
    const AvahiIPv6Address *src_address,
    const AvahiIPv6Address *dst_address,
    uint16_t dst_port)
{
    return -1;  // Not supported on ESP32
}

AvahiDnsPacket *avahi_recv_dns_packet_ipv6(
    int fd,
    AvahiIPv6Address *ret_src_address,
    uint16_t *ret_src_port,
    AvahiIPv6Address *ret_dst_address,
    AvahiIfIndex *ret_iface,
    uint8_t *ret_ttl)
{
    return NULL;  // Not supported on ESP32
}
