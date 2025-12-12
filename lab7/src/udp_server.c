#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int sockfd, n;

  if (argc < 3) {
    printf("Usage: %s <listen_port> <bufsize>\n", argv[0]);
    exit(1);
  }

  int SERV_PORT = atoi(argv[1]);
  int BUFSIZE = atoi(argv[2]);

  char mesg[BUFSIZE], ipadr[16];
  struct sockaddr_in servaddr;
  struct sockaddr_in cliaddr;

  socklen_t slen = sizeof(struct sockaddr_in);

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket problem");
    exit(1);
  }

  memset(&servaddr, 0, slen);
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERV_PORT);

  if (bind(sockfd, (struct sockaddr *)&servaddr, slen) < 0) {
    perror("bind problem");
    exit(1);
  }
  printf("SERVER starts...\n");

  while (1) {
    unsigned int len = slen;

    if ((n = recvfrom(sockfd, mesg, BUFSIZE, 0, (struct sockaddr *)&cliaddr, &len)) < 0) {
      perror("recvfrom");
      exit(1);
    }
    mesg[n] = 0;

    printf("REQUEST %s      FROM %s : %d\n", mesg,
           inet_ntop(AF_INET, (void *)&cliaddr.sin_addr.s_addr, ipadr, 16),
           ntohs(cliaddr.sin_port));

    if (sendto(sockfd, mesg, n, 0, (struct sockaddr *)&cliaddr, len) < 0) {
      perror("sendto");
      exit(1);
    }
  }
}
