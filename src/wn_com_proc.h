#ifndef _WN_COMMANDS_H
#define _WN_COMMANDS_H

// for now, let's keep the command process functions here; it'd be
// better to split later

int wn_com_nothing(int player, int* params);
int wn_com_incoming_msgs(int player, int* params);
int wn_com_list_update(int player, int* params);
int wn_com_adjudicate(int player, int* params);
int wn_com_kick(int player, int* params);
int wn_com_logout(int player, int *params);
int wn_com_read_msgs(int player, int* params);
int wn_com_get_usr_level(int player, int* params);
int wn_com_get_usr_id(int player, int* params);

#endif
