#ifndef _CRYPT_MD5_H
#define _CRYPT_MD5_H

char *crypt_md5(const char *pw, const char *salt);

// pw is the password string as entered
// hex is the ASCII hex string from the database
// computes simple MD5 digest on characters of pw and tests for match
int match_md5(const char *pw, char *hex);

#endif
