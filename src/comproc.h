#ifndef _COMPROC_H
#define _COMPROC_H

#include "command.h"

int com_admins(int p, param_list param);
int com_more(int p, param_list param);
int com_quit(int p, param_list param);
int com_set(int p, param_list param);
int FindPlayer(int p, char* name, int *p1, int *connected);
int com_stats(int p, param_list param);
int com_password(int p, param_list param);
int com_uptime(int p, param_list param);
int com_date(int p, param_list param);
int com_llogons(int p, param_list param);
int com_logons(int p, param_list param);
void AddPlayerLists (int p1, char *ptmp);
int com_who(int p, param_list param);
int com_open(int p, param_list param);
int com_simopen(int p, param_list param);
int com_bell(int p, param_list param);
int com_flip(int p, param_list param);
int com_style(int p, param_list param);
int com_promote(int p, param_list param);
void alias_add(int p, const char *name, const char *value);
int com_alias(int p, param_list param);
int com_unalias(int p, param_list param);
int com_handles(int p, param_list param);
int com_getgi(int p, param_list param);
int com_getpi(int p, param_list param);
int com_checkIP(int p, param_list param);
int com_limits(int p, param_list param);

#endif
