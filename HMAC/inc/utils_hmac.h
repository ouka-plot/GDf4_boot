/* utils_hmac.h - HMAC and base64 prototypes for project */
#ifndef UTILS_HMAC_H_
#define UTILS_HMAC_H_

#include <stdint.h>

void utils_hmac_md5(const char *msg, int msg_len, char *digest, const char *key, int key_len);
void utils_hmac_sha1(const char *msg, int msg_len, char *digest, const char *key, int key_len);
void utils_hmac_sha1_hex(const char *msg, int msg_len, char *digest, const char *key, int key_len);
int base64_decode(const char *base64, unsigned char *bindata);
char *base64_encode(const unsigned char *bindata, char *base64, int binlength);

#endif /* UTILS_HMAC_H_ */

