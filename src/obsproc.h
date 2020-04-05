/* Revision history:
   name		email		yy/mm/dd	Change
   Dave Herscovici		95/11/26	Created
*/

#ifndef _OBSPROC_H
#define _OBSPROC_H

#define MAX_JOURNAL 12

int GameNumFromParam(int p, int *p1, parameter *param);
int com_games(int p, param_list param);
void unobserveAll(int p);
int com_unobserve(int p, param_list param);
int com_observe(int p, param_list param);
int com_allobservers(int p, param_list param);
int com_unexamine(int p, param_list param);
int com_mexamine(int p, param_list param);
int com_moves(int p, param_list param);
void ExamineScratch(int p,  param_list param,int setup);
int com_wname(int p, param_list param);
int com_bname(int p, param_list param);
int com_examine(int p, param_list param);
int com_smoves(int p, param_list param);
int com_sposition(int p, param_list param);
int com_forward(int p, param_list param);
int com_backward(int p, param_list param);
int com_revert(int p, param_list param);
int com_refresh(int p, param_list param);
int com_prefresh(int p, param_list param);

#endif /* _OBSPROC_H */
