// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __VM_SOCKET_H__
#define __VM_SOCKET_H__

#include "vm_types.h"
#include "vm_strings.h"

/*
 * TCP, UDP, and MCAST protocols are supported. The functions are as follows:
 *  vm_socket_init
 *  vm_socket_select/next
 *  vm_socket_accept (for TCP servers only)
 *  vm_socket_read/write
 *  vm_socket_close
 * where vm_socket_init initializes the connection for server or client. The
 * optional functions vm_socket_select and vm_socket_next are used to detect
 * if an accept, read or write operation is non-blocking. For TCP servers,
 * vm_socket_accept accepts an incoming connection and allocates a channel
 * for further communication. Then read/write operations can be performed on
 * the channel. For UDP/MCAST, no channel is required thus the read/write
 * operation can be performed directly. Finally, vm_socket_close closes the
 * socket.
 *
 * Channels
 *
 * A TCP server might accept multiple connections and then read/write to
 * those connections, or channels simutenously. The maximum number of
 * channels a socket can queue is VM_SOCKET_QUEUE. Upon vm_socket_accept,
 * a channel number (1..VM_SOCKET_QUEUE) is returned for subsequent read
 * and write. The channel can be droped by vm_socket_close_chn.
 *
 * For UDP/MCAST or TCP client, always use channel zero.
 *
 * Read/Write
 *
 * There is no garentee on the completeness of reading/writing to a socket.
 * The function vm_socket_read/write always returns the number of bytes
 * actually read/written. A returned value less than 1 indicates an error
 * on the channel and the channel is closed.
 *
 * MCAST Server/Client
 *
 * In the context, a MCAST server refers to the computer broadcasting
 * contents. A MCAST client refers to the computers receiving contents.
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(_SOCKET_SUPPORT)

typedef struct
{
   vm_char *hostname; /* hostname */
   uint16_t port;       /* port number */
} vm_socket_host;

/* Initialize network connection and return socket handle.
 * hd       the returned socket handle.
 * local    the local interface to be binded.
 * remote   the remote interface to be binded.
 * flags    bit-or'ed socket flags
 *
 * In "local"/"remote", hostname can be NULL to indicate any interface.
 * Hostname can be either an IP address or a qualified domain name. Port
 * can be zero to indicate any port. For MCAST, "remote" specifies the
 * multicast group, which must fall into 224.0.0.0-239.255.255.255.
 */
#define VM_SOCKET_UDP    0x1    /* SOCKET_DGRAM   */
#define VM_SOCKET_MCAST  0x2    /* Multicasting   */
#define VM_SOCKET_SERVER 0x4    /* Network Server */

vm_status vm_socket_init(vm_socket *hd,              /* return socket handle */
                         vm_socket_host *local,      /* local binding */
                         vm_socket_host *remote,     /* remote binding */
                         int32_t flags);              /* bit-or'ed flags */

/* Wait until one of the sockets can accept connection or read/write
 * operations on the sockets can be performed without blocking.
 *
 * masks:       bit-or'ed masks
 * hidx:        the index of the socket that signals.
 * chn:         the channel that signals.
 * type:        the signal type
 *
 * The function vm_socket_select blocks until one of the sockets
 * signals and returns the number of signaled sockets. The function
 * vm_socket_next is then used to iterate via all signaled sockets.
 *
 * For portability, always iterate all singaled sockets and perform
 * necessary operations until vm_socket_next returns zero.
 */
#define VM_SOCKET_READ   0x1   /* test for read capability   */
#define VM_SOCKET_WRITE  0x2   /* test for write capability  */
#define VM_SOCKET_ACCEPT 0x4   /* test for accept capability */

int32_t vm_socket_select(vm_socket *handles, int32_t nhandle, int32_t masks);
int32_t vm_socket_next(vm_socket *handles, int32_t nhandle, int32_t *idx, int32_t *chn, int32_t *type);

/* Accept remote connection and return the channel number, to which subsequent
 * read and write are directed. Return -1 if failed.
 */
int32_t vm_socket_accept(vm_socket *handle);

/* Read data from a socket and return the number of bytes actually read.
 * In case of error or eof, return -1 or 0, and the channel is closed.
 * Use channel zero for TCP client, UDP or MCAST.
 */
int32_t vm_socket_read(vm_socket *handle, int32_t chn, void *buffer, int32_t nbytes);

/* Write data to a socket and return the number of bytes actually written.
 * In case of error or eof, return -1 or 0, and the channel is closed.
 * Use channel zero for TCP client, UDP or MCAST.
 */
int32_t vm_socket_write(vm_socket *handle, int32_t chn, void *buffer, int32_t nbytes);

/* clean up a socket channel */
void vm_socket_close_chn(vm_socket *handle, int32_t chn);

/* clean up all socket connections */
void vm_socket_close(vm_socket *handle);

/* get client IP for successefully initialized socket
* In case of error return -1.
*/
int32_t vm_socket_get_client_ip(vm_socket *handle, uint8_t* buffer, int32_t len);

/*****************************************************************************/
/** Old part **/
#if defined(_WIN32) || defined (_WIN64) || defined(_WIN32_WCE)

typedef int32_t socklen_t;

int32_t vm_sock_startup(vm_char a, vm_char b);
int32_t vm_sock_cleanup(void);
int32_t vm_sock_host_by_name(vm_char *name, struct in_addr *paddr);

#define vm_sock_get_error() WSAGetLastError()

#else /* !(defined(_WIN32) || defined (_WIN64) || defined(_WIN32_WCE)) */

#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

typedef int32_t SOCKET;
enum
{
    INVALID_SOCKET              =-1,
    SOCKET_ERROR                =-1
};

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr * LPSOCKADDR;

#define vm_sock_startup(_a,_b) (0)
#define vm_sock_cleanup() (0)
#define closesocket close

#define vm_sock_get_error() errno

int32_t vm_sock_host_by_name(vm_char *name, struct in_addr *paddr);

#endif /* defined(_WIN32) || defined (_WIN64) || defined(_WIN32_WCE) */

#endif /* defined(_SOCKET_SUPPORT) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VM_SOCKET_H__ */
