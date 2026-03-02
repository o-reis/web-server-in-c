#include "uds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>

// Sends a file descriptor over a Unix domain socket using SCM_RIGHTS
int send_fd(int socket, int fd_to_send)
{
    struct msghdr msg = {0};
    char buf[1];
    struct iovec iov[1];

    union
    {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    memset(&u, 0, sizeof(u));

    buf[0] = 'F';
    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *)CMSG_DATA(cmsg)) = fd_to_send;

    if (sendmsg(socket, &msg, 0) < 0)
    {
        perror("sendmsg");
        return -1;
    }
    return 0;
}

// Receives a file descriptor from a Unix domain socket using SCM_RIGHTS
int recv_fd(int socket)
{
    struct msghdr msg = {0};
    char buf[1];
    struct iovec iov[1];
    int received_fd = -1;

    union
    {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    if (recvmsg(socket, &msg, 0) < 0)
    {
        perror("recvmsg");
        return -1;
    }

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int)))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
        {
            received_fd = *((int *)CMSG_DATA(cmsg));
        }
    }
    return received_fd;
}