#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_socklen_t.h>
#include <sys/_types/_u_int32_t.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "./utils.h"

// Interrupt Handler to stop the server
static volatile int running = 1;
void sigintHandler() {
  printf("Stopping...\n");
  running = 0;
}


char buffer[1024];

// Set server options
int setServerOpts(int server_fd) {
  int res;
  if (setFdOptNonBlocking(server_fd))
    return -1;
  if (setFdOptsReuse(server_fd))
    return -1;
  return 0;
}

int startServer(int server_fd, int port, u_int32_t addr,
                struct sockaddr_in **p_address) {
  // bind
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = addr;
  address.sin_port = port;
  *p_address = &address;
  socklen_t addrlen = sizeof(address);
  if (bind(server_fd, (struct sockaddr *)&address, addrlen) < 0) {
    int errsv = errno;
    printf("couldn't BIND the socket to the port (%d)\n", errsv);
    return -1;
  }

  // listen
  if (listen(server_fd, 10) < 0) {
    int errsv = errno;
    printf("couldn't LISTEN (%d)\n", errsv);
    return -1;
  }

  return 0;
}

int createServer(int port, u_int32_t iface, struct sockaddr_in **p_address) {
  // create fd
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    printf("couldn't create socket\n");
    return -1;
  }

  // set options
  int optsResult = setServerOpts(server_fd);
  if (optsResult < 0) {
    printf("couldn't set options on socket\n");
    return optsResult;
  }

  // bind and listen
  int startResult = startServer(server_fd, port, iface, p_address);
  if (startResult < 0) {
    printf("couldn't start server\n");
    return startResult;
  }

  return server_fd;
}

int acceptNewConnections(int server_fd, struct sockaddr *address,
                         struct pollfd fds[], int *nfds) {
  socklen_t addrlen = sizeof(*address);
  while (1) {
    int connection = accept(server_fd, address, &addrlen);

    if (connection <= 0) {
      int errsv = errno;
      if (errsv != EWOULDBLOCK) {
        printf("couldn't ACCEPT a connection on the socket (%d)\n", errsv);
        running = 0;
        return -1;
      }

      // If EWOULDBLOCK, we have accepted all connections in queue
      return 0;
    }

    // We got a new connection
    fds[*nfds].fd = connection;
    fds[*nfds].events = POLLIN;
    *nfds += 1;

    char *peer = getPeerIdentifier(connection);
    printf("Got a new connection from %s\n", peer);
  }
}

int handleConnectionRead(struct pollfd *fds, int i, int *handle_deletes) {
  int fd = fds[i].fd;

  char* peer = getPeerIdentifier(fd);

  if (fds[i].revents & POLLHUP) {
    printf("Connection closed by %s\n", peer);
    close(fds[i].fd);
    fds[i].fd = -1;
    *handle_deletes = 1;
    return 0;
  }
  int close_connection = 0;
  int len, rc;
  while (1) {
    rc = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (rc < 0) {
      int errsv = errno;
      if (errsv != EWOULDBLOCK) {
        printf("Failed to read from connection to %s: %d\n", peer, errsv);
        close_connection = 1;
      }
      break;
    }

    if (rc == 0) {
      printf("Connection closed by %s\n", peer);
      close_connection = 1;
      break;
    }

    len = rc;
    printf("Received data from %s\n", peer);

    rc = send(fd, buffer, len, 0);
    if (rc < 0) {
      printf("Connection closed by %s\n", peer);
      close_connection = 1;
      break;
    }
  }

  if (close_connection) {
    close(fd);
    fds[i].fd = -1;
    *handle_deletes = 1;
  }

  return 0;
}

void compressFds(struct pollfd* fds, int *npollfds) {
    int i = 0, j = 0;
    int nfds = *npollfds;
    for (i = 0; i < nfds; ++i) {
      if (fds[i].fd == -1) {
        *npollfds -= 1;
      } else {
        fds[j].fd = fds[i].fd;
        ++j;
      }
    }
}

// ================
// Event Loop
// ================

int mainLoop(int server_fd, struct sockaddr_in *address) {

  struct pollfd fds[1024];
  int npollfds = 1;

  memset(fds, 0, sizeof(fds));
  fds[0].fd = server_fd;
  fds[0].events = POLLIN;

  int timeout = 100;

  while (running) {
    int rc = poll(fds, npollfds, timeout);

    if (rc < 0) {
      int errsv = errno;
      if(errsv == EINTR) {
          continue;
      }
      printf("poll failed (%d)", errsv);
      closeConnections(fds, npollfds);
      return 1;
    }

    if (rc == 0) {
      // timeout, keep trying
      continue;
    }

    int current_fds = npollfds;
    int handle_deletes = 0;

    for (int i = 0; i < current_fds; ++i) {
      if (fds[i].revents == 0) {
        continue;
      }

      if (!(fds[i].revents & POLLIN)) {
        printf("Unexpected revent %d. Closing server.\n", fds[i].revents);
        closeConnections(fds, npollfds);
        running = 0;
        return -1;
      }

      if (fds[i].fd == server_fd) {
        // Accept a new connection
        if (acceptNewConnections(server_fd, (struct sockaddr *)address, fds,
                                 &npollfds)) {
          printf("Unexpected failed connection. Closing server.");
          closeConnections(fds, npollfds);
          running = 0;
          return -1;
        }
      } else {
        // socket connection
        if (handleConnectionRead(fds, i, &handle_deletes)) {
          printf("Unexpected connection handling error. Closing server.\n");
          closeConnections(fds, npollfds);
          running = 0;
          return -1;
        }
      }
    }

    if (handle_deletes) {
    // compress the list of fds by overwriting the fds as needed
    // note that we only change the fds as all events are POLLIN
    compressFds(fds, &npollfds);
    }
  }
  return 0;
}

// ================
// Argparse Utils
// ================

int getPortOrDefault(int argc, char **argv) {
  // htoens fixes the endianness of port
  int default_port = htons(3000);
  if (argc < 2) {
    printf("using default port 3000 as port wasn't provided\n");
    return default_port;
  }

  int port = 3000;
  if(getPort(argv[1], &port)) {
      printf("couldn't convert specified port %s to a valid port number... defaulting to 3000.\n", argv[1]);
      return default_port;
  }

  return port;
}

u_int32_t getIface(int argc, char **argv) {
  if (argc < 3) {
    printf("listening on all interfaces as interface wasn't specified\n");
    return INADDR_ANY;
  }

  u_int32_t addr = getIpAddrFromStr(argv[2]);
  return addr;
}

// ================
// Main
// ================

int main(int argc, char **argv) {
  int port = getPortOrDefault(argc, argv);
  u_int32_t iface = getIface(argc, argv);

  // set up SIGINT handler
  signal(SIGINT, sigintHandler);

  // create server
  struct sockaddr_in *address;
  int server_fd = createServer(port, iface, &address);
  if (server_fd < 0) {
    printf("Initializing server failed!\n");
    return -1;
  }

  // notify that we're up
  char* selfid = getSelfIdentifier(server_fd);
  printf("listening on %s\nplease send SIGINT / ctrl-c to stop\n\n", selfid);

  // start event loop
  int main_loop_result = mainLoop(server_fd, address);

  // close the server
  close(server_fd);

  return main_loop_result;
}
