#ifndef _SETUP_H
#define _SETUP_H

#include "command.h"

int com_setup (int p,param_list param);
int com_tomove (int p,param_list param);
int com_clrsquare (int p,param_list param);
int is_drop(char* dropstr);
int attempt_drop(int p,int g,char* dropstr);

#endif
