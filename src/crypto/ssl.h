// At the moment I have hard coded one buffer. The size
//  is the max size of SSLCiphertext.length (so, actually it should
//  be increased to include the other information in the struct)
// I might need to make a new, or split the current, buffer because
//  I have to have space to read in and write out, as well as keep
//  any data that has not been translated.
// It works for now.
typedef struct _ssl_t{
  char buffer[18456];
  int length;
  int max_size; // can't define const here
  // Current CipherSuite 
  // Client random / Server random ???
  // pointers to different crypto functions
} SSL_t;

int CreateSSLHello(SSL_t *ssl);
int ReadSSLHello(SSL_t *ssl);
void PrintSSLPacket(SSL_t *ssl);
