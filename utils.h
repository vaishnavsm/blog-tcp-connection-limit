#include <sys/_types/_u_int32_t.h>

u_int32_t getIpAddrFromStr(char *addr);
int getPort(char* data, int* port);
void closeConnections(struct pollfd fdlist[], int len);
char *getPeerIdentifier(int fd);
char *getSelfIdentifier(int fd);
int setFdOptNonBlocking(int fd);
int setFdOptsReuse(int fd);
