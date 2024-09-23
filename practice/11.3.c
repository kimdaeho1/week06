#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

int accept(int listenfd, struct sockaddr *addr, int *addrlen);

int listen(int sockfd, int backlog);

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int connect(int clientfd, const struct sockaddr *addr, socklen_t addrlen);

int socket(int domain, int type, int protocool);

/*IP소켓 주소 구조체*/
struct sockaddr_in {
    uint16_t    sin_family; 
    uint16_t    sin_port;
    struct in_addr  sin_sddr;
    unsigned char sin_zero[8];
};

struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};