#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "ssl.h"

int main(int argc, char *argv[])
{
  SSL_t ssl;
  int sockfd, portno, rc;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  portno = 443;
  sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd<0){
    fprintf(stderr,"Error creating socket\n");
    exit(sockfd);
  }

  server = gethostbyname(argv[1]);
  if(server==NULL){
    fprintf(stderr,"Error looking up host %s\n",argv[1]);
    exit(1);
  }

  /**
   *matrixSslOpen()
   *matrixSslReadKeys()
   **/
  printf("Calling CreateSSLHello()\n");
  rc = CreateSSLHello(&ssl);
  printf("Finished calling CreateSSLHello()\n");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
  serv_addr.sin_port = htons(portno);
  if(connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    fprintf(stderr,"ERROR connecting to server\n");
    exit(1);
  }

  PrintSSLPacket(&ssl);

  printf("Write ssl.buffer\n");
  write(sockfd,ssl.buffer,ssl.length);
  printf("Finished writing\n");
  ssl.length = read(sockfd,ssl.buffer,ssl.max_size);
  ReadSSLHello(&ssl);

  /**
   *matrixSslNewSession()
   *matrixSslSetCetValidator()
   *encodeSslHandshake()

   *write handshake buffer

   *readSslResponse() <-+
                        |
   *read return code    |-- similar/same function??
                        |
   *sslEncode()         |
   *sslDecode() <-------+
   
   *encodeSslCloseAlert()
   
   *write close alert buffer
   **/
   close(sockfd);

  /**
   *sslClose()
   * -free connection
   * -free keys
   * -close pki interface
   **/

  return 0;
}
