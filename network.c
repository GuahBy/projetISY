#include "messaging.h"

// ========== Gestion des sockets UDP ==========

int socket_create_udp(void) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erreur socket");
        return -1;
    }
    return sockfd;
}

int socket_bind_udp(int sockfd, int port) {
    struct sockaddr_in servaddr;
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Erreur bind");
        return -1;
    }
    
    printf("Socket lié au port %d\n", port);
    return 0;
}

ssize_t socket_send(int sockfd, Message *msg, struct sockaddr_in *dest_addr) {
    socklen_t len = sizeof(*dest_addr);

    ssize_t n = sendto(sockfd, msg, sizeof(Message), 0,
                       (struct sockaddr *)dest_addr, len);

    if (n < 0) {
        perror("Erreur sendto");
        return -1;
    }

    return n;
}

ssize_t socket_receive(int sockfd, Message *msg, struct sockaddr_in *src_addr) {
    socklen_t len = sizeof(*src_addr);

    ssize_t n = recvfrom(sockfd, msg, sizeof(Message), 0,
                         (struct sockaddr *)src_addr, &len);

    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Erreur recvfrom");
        }
        return -1;
    }

    return n;
}
