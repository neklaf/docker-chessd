#ifndef _SHUTDOWN_H
#define _SHUTDOWN_H

void output_shut_mess(void);
void ShutHeartBeat(void);
int check_and_print_shutdown(int p);
int com_shutdown(int p, param_list param);
int com_whenshut(int p, param_list param);

#endif
