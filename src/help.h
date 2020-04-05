#ifndef _HELP_H
#define _HELP_H

/*  Help and admin help directories for current locale */
extern char *locale_help_dir;
extern char *locale_ahelp_dir;
extern char *locale_usage_dir;

int help_init();
int help_free();
int com_help(int p, param_list param);
int com_ahelp(int p, param_list param);

#endif /* _HELP_H */

