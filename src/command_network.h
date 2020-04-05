#ifndef _COMMAND_NETWORK_H
#define _COMMAND_NETWORK_H

#include "network.h"

int alias_lookup(char *tmp, struct alias_type *alias_list, int numalias);
int process_input(int fd, char *com_string);
int process_new_connection(int fd, struct in_addr fromHost);
int process_disconnection(int fd);
int process_heartbeat(int *fd);

int process_invalid_input(int fd, char *com_string);
int process_net_notification(int fd, int error_type);


void commands_init(void);
void TerminateCleanup(void);
const struct alias_type *alias_list_global(void);
const struct alias_type *alias_list_personal(int p, int *n);
int com_acheckhelp(int p, param_list param);


#endif
