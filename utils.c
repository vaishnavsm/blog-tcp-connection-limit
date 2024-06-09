#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_u_int32_t.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

// IP Addr Utils
u_int32_t getIpAddrFromStr(char *addr) {

  if (addr == NULL) {
    return INADDR_LOOPBACK;
  }

  if (strlen(addr) == 0) {
    return INADDR_LOOPBACK;
  }

  if (strcmp(addr, "0.0.0.0") == 0) {
    return INADDR_ANY;
  }

  return inet_addr(addr);
}

int getPort(char* data, int* port) {
    char *end;
    int tryConvert = strtol(data, &(end), 10);
    if (tryConvert == 0 || *end != 0) {
      return -1;
    } else {
      *port = htons(tryConvert);
      return 0;
    }
}

void closeConnections(struct pollfd fdlist[], int len) {
  for (int i = 0; i < len; ++i) {
    close(fdlist[i].fd);
  }
}

char *getPeerIdentifier(int fd) {
    struct sockaddr_in peeraddr;
    socklen_t peersize = sizeof(peeraddr);
    getpeername(fd, (struct sockaddr *)&peeraddr,  &peersize);
    struct in_addr peer_inaddr;
    peer_inaddr.s_addr = peeraddr.sin_addr.s_addr;
    char* addr = inet_ntoa(peer_inaddr);
    int port = htons(peeraddr.sin_port);
    char *result = malloc(24 * sizeof(char));
    if(sprintf(result, "%s:%u", addr, port) < 0){
        return "failed to get identifier";
    }
    return result;
}

char *getSelfIdentifier(int fd) {
    struct sockaddr_in selfaddr;
    socklen_t selfsize = sizeof(selfaddr);
    getsockname(fd, (struct sockaddr *)&selfaddr,  &selfsize);
    struct in_addr self_inaddr;
    self_inaddr.s_addr = selfaddr.sin_addr.s_addr;
    char* addr = inet_ntoa(self_inaddr);
    int port = htons(selfaddr.sin_port);
    char *result = malloc(24 * sizeof(char));
    if(sprintf(result, "%s:%u", addr, port) < 0){
        return "failed to get identifier";
    }
    return result;
}


//====================================
// FD Options
//====================================

int setFdOptNonBlocking(int fd) {
  // Set timeout to prevent blocking
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    int errsv = errno;
    printf("couldn't set timeout (SO_RCVTIMEO) setting (%d)\n", errsv);
    return -1;
  }

  if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
    int errsv = errno;
    printf("couldn't set timeout (SO_SNDTIMEO) setting (%d)\n", errsv);
    return -1;
  }

  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    return -1;
  flags = flags | O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    int errsv = errno;
    printf("couldn't set O_NONBLOCK setting (%d)\n", errsv);
    return -1;
  }

  return 0;
}

int setFdOptsReuseAddr(int fd) {
  int enable = 1;

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) <
      0) {
    int errsv = errno;
    printf("couldn't set SO_REUSEADDR setting (%d)\n", errsv);
    return -1;
  }

  return 0;
}

int setFdOptsReusePort(int fd) {
  int enable = 1;

  if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) <
      0) {
    int errsv = errno;
    printf("couldn't set SO_REUSEPORT setting (%d)\n", errsv);
    return -1;
  }

  return 0;
}
