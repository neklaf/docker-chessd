/*
* $Author: svmello $
* $Locker:  $
*
* $Revision: 1.1 $
* $Date: 2004/01/12 19:36:14 $
* $Source: /cvsroot/chessd/chessd2/src/seekproc.h,v $
***
* $Log: seekproc.h,v $
* Revision 1.1  2004/01/12 19:36:14  svmello
* merging older version
*
* Revision 1.3  2003/11/11 14:12:19  svmello
* implemented command atsreload, used to reload timeseal
* solved conflicts in seekproc.h
*
* Revision 1.2  2003/10/17 14:26:36  svmello
* Added support for seek / unseek / play / shought from ICS.
* The code is actually compilling, but isn't tested enought.
* I will clean this code later.
*
* Revision 1.2  1997/02/27 16:06:07  chess
* New contant MAX_PLAYER_SEEK
*
* Revision 1.1  1996/11/06 00:18:10  chess
* Initial revision
*
* Revision 1.1  1996/11/04 18:07:21  chess
* Initial revision
*
*/
#ifndef _SEEK_H
#define _SEEK_H

#define MAX_PLAYER_SEEK 3

extern int com_seek(int, param_list);
extern int com_unseek(int, param_list);
extern int com_sought(int, param_list);
extern int com_play(int, param_list);

extern void unseekall();
extern void seekinit();

void seekremoveall (int p);

#endif

