
#ifndef _LISTS_H
#define _LISTS_H

#include "command.h"
#include "adminproc.h"

/* yes, it's all cheesy..  there is no significance to the order, but make
   sure it matches the order in lists.c */

enum ListWhich {
	L_ADMIN = 0, L_REMOVEDCOM, L_FILTER, L_BAN, L_ABUSER,
	L_MUZZLE, L_CMUZZLE, L_C1MUZZLE, L_C24MUZZLE, L_C46MUZZLE, L_C49MUZZLE,
	L_C50MUZZLE, L_C51MUZZLE, L_FM, L_IM, L_GM, L_WGM, L_BLIND, L_TEAMS,
	L_COMPUTER, L_TD,
	L_CENSOR, L_GNOTIFY, L_NOPLAY, L_NOTIFY, L_CHANNEL, L_FOLLOW,
	L_MANAGER, L_C2MUZZLE, L_TEACHER, L_SERVER_MASTER, L_CHMUZZLE,
	L_NAME_ABUSER,

	L_LASTLIST /* this MUST be the last list, add all lists before it */
};

enum ListPerm {P_HEAD = 0, P_GOD, P_ADMIN, P_PUBLIC, P_PERSONAL, P_DEMIGOD, P_MASTER};

typedef struct {
	enum ListPerm rights; 
	char *name;
} ListTable;

struct List {
	enum ListWhich which;
	int numMembers;
	char **m_member; /* _LEN(numMembers) */
	struct List *next;
};


void lists_close(void);
int list_add(int p, enum ListWhich l, const char *s, int loading);
struct List *list_findpartial(int p, char *which, int gonnado);
int in_list(int p, enum ListWhich which, char *member);
int list_addsub(int p, char* list, char* who, int addsub);
int com_addlist(int p, param_list param);
int com_sublist(int p,param_list param);
int com_togglelist(int p,param_list param);
int com_showlist(int p, param_list param);
int list_channels(int p,int p1);
void list_free(struct List * gl);
void lists_validate(int p);
int titled_player(int p,char* name);

#endif   /* _LISTS_H */
