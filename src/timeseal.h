#ifndef _TIMESEAL_H
#define _TIMESEAL_H

#include "network.h"
#include "command.h"
int com_atsreload(int p, param_list param);
int com_timeseal_fail(int p, param_list param);


void timeseal_init();
int timeseal_parse(char *command, struct connection_t *con);
void ExecuteFlagCmd(int p, struct connection_t *con);

void timeseal_setpath(char *s);



#endif
