#ifndef _ADMINPROC_H
#define _ADMINPROC_H

#include "command.h"


int sys_announce(int event_type, int admin_level);

////
int check_admin(int p, int level);
int check_admin2(int p1, int p2);
int check_admin_level(int p, int level);
int com_adjudicate(int p, param_list param);
int com_remplayer(int p, param_list param);
int com_raisedead(int p, param_list param);
int com_addplayer(int p, param_list param);
int com_pose(int p, param_list param);
int com_asetv(int p, param_list param);
int com_announce(int p, param_list param);
int com_annunreg(int p, param_list param);
int com_asetpasswd(int p, param_list param);
int com_asetemail(int p, param_list param);
int com_asetrealname(int p, param_list param);
int com_asethandle(int p, param_list param);
int com_asetadmin(int p, param_list param);
int com_asetblitz(int p, param_list param);
int com_asetwild(int p, param_list param);
int com_asetstd(int p, param_list param);
int com_asetlight(int p, param_list param);
int com_asetbug(int p, param_list param);
int com_ftell(int p, param_list param);
int com_nuke(int p, param_list param);
int com_summon(int p, param_list param);
int com_addcomment(int p, param_list param);
int com_showcomment(int p, param_list param);
int com_admin(int p, param_list param);
int com_hideinfo(int p, param_list param);
int com_quota(int p, param_list param);
int com_areload(int p, param_list param);
int com_realname(int p, param_list param);
int com_invisible(int p, param_list param);

#endif
