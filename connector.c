#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
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

int setClientOpts(int client_fd) {
  int res;
  // if (setFdOptNonBlocking(client_fd))
    // return -1;
  if (setFdOptsReuse(client_fd))
    return -1;
  return 0;
}

int startClient(int client_fd, int port, u_int32_t addr,
                struct sockaddr_in **p_address) {
  // bind
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = addr;
  address.sin_port = port;
  *p_address = &address;
  socklen_t addrlen = sizeof(address);
  if (bind(client_fd, (struct sockaddr *)&address, addrlen) < 0) {
    int errsv = errno;
    printf("couldn't BIND the socket to the port (%d)\n", errsv);
    return -1;
  }
  return 0;
}

int createClient(int port, u_int32_t iface, struct sockaddr_in **p_address) {
    // create fd
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
      printf("couldn't create socket\n");
      return -1;
    }

    // set options
    int optsResult = setClientOpts(client_fd);
    if (optsResult < 0) {
      printf("couldn't set options on socket\n");
      return optsResult;
    }

    // bind and listen
    int startResult = startClient(client_fd, port, iface, p_address);
    if (startResult < 0) {
      printf("couldn't start client\n");
      return startResult;
    }

    return client_fd;
}

int connectToServer(int client_fd, u_int32_t addr, int port) {
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = addr;
    address.sin_port = port;
    socklen_t addrlen = sizeof(address);
    if(connect(client_fd, (struct sockaddr*)&address, addrlen) < 0) {
        int errsv = errno;
        printf("Failed to connect to server (%d)\n", errsv);
        return -1;
    }
    return 0;
}

char buffer[1024];
int handleError(int rc) {
    if(rc <= 0) {
        if(rc == 0) {
            printf("Server closed connection\n");
        } else {
            int errsv = errno;
            printf("Error sending data to server (%d)\n", errno);
        }
        running = 0;
        return 1;
    }
    return 0;
}

int mainLoop(int fd, char* data) {
    // Send a request every 5 seconds
    // Print any data the server sends us
    int size = strlen(data);
    int rc = 0;
    while(running) {
        printf("Lub %s... ", data);
        if(handleError(send(fd, data, strlen(data), 0))) break;
        if(handleError(recv(fd, buffer, sizeof(buffer), 0))) break;
        printf("Dub... %s\n", buffer);
        sleep(5);
    }
    return 0;
}

void printHelpMessage() {
    printf("Welcome to the connector\n");
    printf("Syntax:\n");
    printf("connector <connector ip> <connector port> <server ip> <server port> <data = hi>\n");
    printf("\n");
    printf("example:\n");
    printf("connector 127.0.0.1 3001 127.0.0.1 3000\n");
    printf("tries to connect to 127.0.0.1:3000 from port 3001 on loopback\n");
}

int main(int argc, char **argv) {
    if(argc < 5) {
        printHelpMessage();
        return 0;
    }

  u_int32_t client_addr = getIpAddrFromStr(argv[1]);
  int client_port = 0;
  if(getPort(argv[2], &client_port)) {
      printf("Couldnt parse self port %s\n", argv[2]);
      return 1;
  }
  u_int32_t server_iface = getIpAddrFromStr(argv[3]);
  int server_port = 0;
  if(getPort(argv[4], &server_port)) {
      printf("Couldnt parse server port %s\n", argv[4]);
      return 1;
  }

  char* data;
  if(argc < 6) {
      data = "hi";
  } else {
      data = argv[5];
  }

  // set up SIGINT handler
  signal(SIGINT, sigintHandler);

  // create server
  struct sockaddr_in *address;
  int client_fd = createClient(client_port, client_addr, &address);
  if (client_fd < 0) {
    printf("Initializing client failed!\n");
    return -1;
  }

  char* selfid = getSelfIdentifier(client_fd);

    if(connectToServer(client_fd, server_iface, server_port)) {
        return 1;
    }

  printf("bound on %s and connected to %s:%s\nplease send SIGINT / ctrl-c to stop\n\n", selfid, argv[3], argv[4]);

  int main_loop_result = mainLoop(client_fd, data);

  close(client_fd);

  return main_loop_result;
}
