#ifndef __CONFIG__FILE__READ__
#define __CONFIG__FILE__READ__

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
	This small function is used to parse .conf files,
	returning an array in the form of
		key, value
	entries.
 */
#include <ctype.h>

typedef struct config_key {
	char *name;
	char *value;
} config_key;

/*

 */
int read_config(char* filename, config_key** result);

#endif
