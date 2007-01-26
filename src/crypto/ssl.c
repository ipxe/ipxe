#if 0

#include "ssl.h"
#include "ssl_constructs.h"
#include <string.h> // for bcopy()
#include <time.h> // for time()
#include <stdlib.h> // for rand(), htons?, htonl?
// note net byte order is big-endian
// Need to set error codes

int CreateSSLHello(SSL_t *ssl)
{
  printf("In CreateSSLHello()\n",ssl);

  // Initalize the structure
  bzero(ssl,sizeof(SSL_t));
  //ssl->max_size = sizeof(ssl->buffer);
  ssl->max_size = 18456;

  // Declare variables
  int i; void *ptr;

  // Set pointers into buffer
  SSLPlaintext *record = (SSLPlaintext *)ssl->buffer;
  Handshake *handshake = (Handshake *)record->fragment;
  // the body starts right after the handshake
  printf("sizeof(Handshake) = %d\n",sizeof(Handshake));
  ClientHello *hello = (ClientHello *)(handshake + 1);

  printf("record->%#x, handshake->%#x, hello->%#x\n",record,handshake,hello);

  // Construct ClientHello Message
  hello->client_version = version;
  i = htonl(time(NULL));
  bcopy(&i,hello->random.gmt_unix_time,4);
  for(i=0;i<28;i++){ hello->random.random_bytes[i] = (uint8)rand(); }
  hello->session_id_length = 0;
  hello->session_id = &hello->session_id_length;
  hello->session_id_end = hello->session_id;
  hello->cipher_suites_length = (CipherSuiteLength *)(hello->session_id_end + 1);
  hello->cipher_suites = (hello->cipher_suites_length + 1);
  hello->cipher_suites_end = hello->cipher_suites;
  i = htons(2*5); // 2 bytes per Suite * 5 Suites
  bcopy(&i,hello->cipher_suites_length,2);
  bcopy(SSL_NULL_WITH_NULL_NULL,hello->cipher_suites_end,sizeof(CipherSuite));
  *hello->cipher_suites_end++;
  bcopy(SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,hello->cipher_suites_end,sizeof(CipherSuite));
  *hello->cipher_suites_end++;
  bcopy(SSL_DH_DSS_WITH_DES_CBC_SHA,hello->cipher_suites_end,sizeof(CipherSuite));
  *hello->cipher_suites_end++;
  bcopy(SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,hello->cipher_suites_end,sizeof(CipherSuite));
  *hello->cipher_suites_end++;
  bcopy(SSL_DH_anon_WITH_RC4_128_MD5,hello->cipher_suites_end,sizeof(CipherSuite));
  hello->compression_methods_length = (CompressionMethodLength *)(hello->cipher_suites_end + 1);
  hello->compression_methods = (hello->compression_methods_length + 1);
  hello->compression_methods_end = hello->compression_methods;
  *hello->compression_methods_length = 1;
  *hello->compression_methods_end = compression_method_null;

  // Construct Handshake Message
  handshake->msg_type = handshake_type_client_hello;
  i = (void *)(hello->compression_methods_end + 1) - (void *)hello;
  printf("Handshake.length = %d\n", i);
  handshake->length[0] = (char)*(&i+8);
  handshake->length[1] = (char)*(&i+8);
  handshake->length[2] = (char)i;
  //bcopy((&i+1),handshake->length,3); // +1 so we copy 3 bytes

  // Construct SSL Record
  printf("sizeof(ContentType)=%d\n",sizeof(ContentType));
  printf("sizeof(uint8)=%d\n",sizeof(uint8));
  record->type = content_type_handshake;
  record->version = version;
  i += sizeof(Handshake);
  printf("SSLPlaintext.length = %d\n",i);
  record->length[0] = (char)*(&i+8);
  record->length[1] = (char)i;
  //bcopy(&i,record->length,4); // length of handshake

  // Set total size of message
  i += sizeof(ContentType) + sizeof(ProtocolVersion) + sizeof(uint16);
  ssl->length = i;
  printf("End of CreateSSLHello\n");
  return 0;
}

void PrintSSLPacket(SSL_t *ssl)
{
  printf("Printing packet with length:%d\n", ssl->length);
  char *ptr = ssl->buffer;
  char *begin = ptr;
  char *tmp;
  char *end = ssl->buffer + ssl->length;
  printf("Record Layer:\n");
  printf("\tContentType: %2hhX\n",(char)*ptr++);
  printf("\tVersion: %2hhX %2hhX\n", (char)*ptr++, (char)*ptr++);
  printf("\tLength: %2hhX %2hhX\n", (char)*ptr++, (char)*ptr++);

  printf("Handshake:\n");
  printf("\tType: %2hhX\n", (char)*ptr++);
  printf("\tLength: %2hhX %2hhX %2hhX\n", (char)*ptr++, (char)*ptr++, (char)*ptr++);
  printf("\tVersion: %2hhX %2hhX\n", (char)*ptr++, (char)*ptr++);
  printf("\tgmt_unix_time: %2hhX %2hhX %2hhX %2hhX\n", (char)*ptr++, (char)*ptr++, (char)*ptr++, (char)*ptr++);
  printf("\trandom: ");
  tmp = ptr + 28;
  for(;ptr<tmp;ptr++){printf("%2hhX ", (char)*ptr);}

  printf("\n\nHexDump:\n");

  int ctr = 0;
  for(;begin<end;begin++){printf("%2hhX ",(char)*begin);if(++ctr%10==0){printf("\n");}}
  printf("\n\n");
}

int ReadSSLHello(SSL_t *ssl)
{
  SSLCiphertext *ct = (SSLCiphertext *)ssl->buffer;

  if(ct->type == content_type_alert){
    // assuming text is still plaintext
    Alert *a = (Alert *)&ct->fragment;
    if(a->level == alert_level_fatal){
      printf("Fatal Alert %d, connection terminated\n",a->description);
      return (1);
    }else if(a->level == alert_level_warning){
      printf("Warning Alert %d\n", a->description);
    }else{
      printf("Unknown alert level %d\n", a->level);
    }
  }else{
    printf("SSL type %d\n",ct->type);
  }
  return (0);
}

#endif
