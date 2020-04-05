#ifndef _CRYPT_MD5C_H
#define _CRYPT_MD5C_H

#include "md5.h"

void MD5Init(MD5_CTX *context);
void MD5Update(MD5_CTX *context,
	       const unsigned char *input,
	       unsigned int inputLen);
void MD5Final(unsigned char digest[16],
	      MD5_CTX *context);

#endif
