/*
   Copyright (c) 1993 Richard V. Nash.
   Copyright (c) 2000 Dan Papasian
   Copyright (C) Andrew Tridgell 2002
   Copyright (c) 2003 Federal University of Parana

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "config_genstrc.h"
#include "utils.h"
#include "globals.h"
#include "variable.h"
#include "chessd_locale.h"
#include "gameproc.h"
#include "malloc.h"
#include "formula.h"
#include "comproc.h"

static int set_boolean_flag(int p, char *val, unsigned long flag);
static int set_availinfo(int p, char *var, char *val);
static int set_availmax(int p, char *var, char *val);
static int set_availmin(int p, char *var, char *val);
static int set_open(int p, char *var, char *val);
static int set_tourney(int p, char *var, char *val);
static int set_sopen(int p, char *var, char *val);
static int set_ropen(int p, char *var, char *val);
static int set_rated(int p, char *var, char *val);
static int set_shout(int p, char *var, char *val);
static int set_ads(int p, char *var, char *val);
static int set_cshout(int p, char *var, char *val);
static int set_kibitz(int p, char *var, char *val);
static int set_kiblevel(int p, char *var, char *val);
static int set_tell(int p, char *var, char *val);
static int set_notifiedby(int p, char *var, char *val);
static int set_pinform(int p, char *var, char *val);
static int set_ginform(int p, char *var, char *val);
static int set_helpme(int p, char *var, char *val);
static int set_bell(int p, char *var, char *val);
static int set_highlight(int p, char *var, char *val);
static int set_style(int p, char *var, char *val);
static int set_flip(int p, char *var, char *val);
static int set_time(int p, char *var, char *val);
static int set_inc(int p, char *var, char *val);
static int set_language (int p, char *var, char *val);
static int set_promote(int p, char *var, char *val);
static int set_interface(int p, char *var, char *val);
static int set_prompt(int p, char *var, char *val);
static int RePartner (int p, int new);
static int set_busy(int p, char *var, char *val);
static int set_formula(int p, char *var, char *val);
static int set_find(char *var);


/*	gabrielsan: (21/01/2004)
 *		Some of these variables should be removed because they are
 *		related to features that won't be available anymore
 *
 */
static var_list variables[] = {
  {"ads", set_ads},
  {"availinfo", set_availinfo},
  {"availmax", set_availmax},
  {"availmin", set_availmin},
  {"open", set_open},
  {"tourney", set_tourney},
  {"simopen", set_sopen},
  {"rated", set_rated},
  {"ropen", set_ropen},
  {"cshout", set_cshout},
  {"shout", set_shout},
  {"kibitz", set_kibitz},
  {"kiblevel", set_kiblevel},
  {"tell", set_tell},
  {"notifiedby", set_notifiedby},
  {"pinform", set_pinform},
  {"i_login", set_pinform},
  {"ginform", set_ginform},
  {"i_game", set_ginform},
  {"bell", set_bell},
  {"helpme", set_helpme},
  {"highlight", set_highlight},
  {"style", set_style},
  {"flip", set_flip},
  {"prompt", set_prompt},
  {"promote", set_promote},
  {"time", set_time},
  {"inc", set_inc},
  {"language", set_language},
  {"busy", set_busy},
  {"f1", set_formula},
  {"f2", set_formula},
  {"f3", set_formula},
  {"f4", set_formula},
  {"f5", set_formula},
  {"f6", set_formula},
  {"f7", set_formula},
  {"f8", set_formula},
  {"f9", set_formula},
  {"formula", set_formula},
  {"interface", set_interface},
  {NULL, NULL}
};

static int set_boolean_flag(int p, char *val, unsigned long flag)
{
  int v = -1;

  if (val == NULL) {
    TogglePFlag(p, flag);
    v = BoolCheckPFlag(p, flag);
    return (v);
  }
  if (sscanf(val, "%d", &v) != 1) {
    stolower(val);
    if (!strcmp(val, "off") || !strcmp(val, "false"))
      v = 0;
    else if (!strcmp(val, "on") || !strcmp(val, "true"))
      v = 1;
  }
  SetPFlag(p, flag, v);
  return v;
}

static int set_availinfo(int p, char *var, char *val)
{
  int v = set_boolean_flag(p, val, PFLAG_AVAIL);

  if (v < 0)
    return VAR_BADVAL;
   pprintf(p, _("availinfo set to %d.\n"), BoolCheckPFlag(p, PFLAG_AVAIL));

  if (v > 0)
    pprintf(p, _("You will receive info on who is available to play.\n"));
  else
    pprintf(p, _("You are no longer receiving info on who is available to play.\n"));
  return VAR_OK;
}

static int set_availmax(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 9999))
    return VAR_BADVAL;

  if (v < pp->availmin) {
    pprintf(p, _("You can't set availmax to less than availmin.\n"));
    return VAR_OK;
  }

  if ((pp->availmax = v)) {
    pprintf(p, _("You will be notified of availability with blitz ratings %d - %d.\n"),pp->availmin,v);
  } else
    pprintf(p, _("You will be notified of all available players.\n"));
  return VAR_OK;
}

static int set_availmin(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 9999))
    return VAR_BADVAL;

  if (v > pp->availmax) {
    pprintf(p, _("You can't set availmin to more than availmax.\n"));
    return VAR_OK;
  }

  pp->availmin = v;
  pprintf(p, _("You will be notified of availability with blitz ratings %d - %d.\n"),v,pp->availmax);
  return VAR_OK;
}

static int set_open(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = set_boolean_flag(p, val, PFLAG_OPEN);

  if (v < 0)
    return VAR_BADVAL;
  if (v > 0) {
    pprintf(p, _("You are now open to receive match requests.\n"));
    announce_avail (p);
  } else {
    decline_withdraw_offers(p, -1, PEND_MATCH,DO_DECLINE | DO_WITHDRAW);
    pprintf(p, _("You are no longer receiving match requests.\n"));
    if (pp->game < 0)
      announce_notavail (p);
  }
  return VAR_OK;
}

static int set_tourney(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = set_boolean_flag(p, val, PFLAG_TOURNEY);

  if (v < 0)
    return VAR_BADVAL;
  if (v > 0) {
    decline_withdraw_offers(p, -1, PEND_MATCH,DO_DECLINE | DO_WITHDRAW);
    pprintf(p, _("Your tournament variable is now set.\n"));
    announce_notavail (p);
  } else {
    pprintf(p, _("Your tournament variable is no longer set.\n"));
    if (pp->game < 0)
      announce_avail (p);
  }
  return VAR_OK;
}

static int set_sopen(int p, char *var, char *val)
{
  int v = set_boolean_flag(p, val, PFLAG_SIMOPEN);

  if (v < 0)
    return VAR_BADVAL;
  pprintf(p, _("sopen set to %d.\n"), v);

  if (v > 0)
    pprintf(p, _("You are now open to receive simul requests.\n"));
  else
    pprintf(p, _("You are no longer receiving simul requests.\n"));
  decline_withdraw_offers(p, -1, PEND_SIMUL,DO_DECLINE);
  return VAR_OK;
}

static int set_ropen(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_ROPEN) < 0)
    return VAR_BADVAL;
  pprintf(p, _("ropen set to %d.\n"), BoolCheckPFlag(p, PFLAG_ROPEN));
  return VAR_OK;
}

static int set_rated(int p, char *var, char *val)
{
  if (!CheckPFlag(p, PFLAG_REG)) {
    pprintf(p, _("You cannot change your rated status.\n"));
    return VAR_OK;
  }
  if (set_boolean_flag(p, val, PFLAG_RATED) < 0)
    return VAR_BADVAL;
  pprintf(p, _("rated set to %d.\n"), BoolCheckPFlag(p, PFLAG_RATED));
  return VAR_OK;
}

static int set_shout(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_SHOUT) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_SHOUT))
    pprintf(p, _("You will now hear shouts.\n"));
  else
    pprintf(p, _("You will not hear shouts.\n"));
  return VAR_OK;
}

static int set_ads(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_ADS) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_ADS))
    pprintf(p, _("You will now hear ads.\n"));
  else
    pprintf(p, _("You will not hear ads.\n"));
  return VAR_OK;
}

static int set_cshout(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_CSHOUT) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_CSHOUT))
    pprintf(p, _("You will now hear cshouts.\n"));
  else
    pprintf(p, _("You will not hear cshouts.\n"));
  return VAR_OK;
}

static int set_kibitz(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_KIBITZ) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_KIBITZ))
    pprintf(p, _("You will now hear kibitzes.\n"));
  else
    pprintf(p, _("You will not hear kibitzes.\n"));
  return VAR_OK;
}
static int set_kiblevel(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 9999))
    return VAR_BADVAL;
  pp->kiblevel = v;
  pprintf(p, _("Kibitz level now set to: %d.\n"), v);
  return VAR_OK;
}

static int set_tell(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_TELL) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_TELL))
    pprintf(p, _("You will now hear tells from unregistered users.\n"));
  else
    pprintf(p, _("You will not hear tells from unregistered users.\n"));
  return VAR_OK;
}

static int set_notifiedby(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_NOTIFYBY) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_NOTIFYBY))
    pprintf(p, _("You will now hear if people notify you, but you don't notify them.\n"));
  else
    pprintf(p, _("You will not hear if people notify you, but you don't notify them.\n"));
  return VAR_OK;
}

static int set_pinform(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_PIN) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_PIN))
    pprintf(p, _("You will now hear logins/logouts.\n"));
  else
    pprintf(p, _("You will not hear logins/logouts.\n"));
  return VAR_OK;
}

static int set_ginform(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_GIN) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_GIN))
    pprintf(p, _("You will now hear game results.\n"));
  else
    pprintf(p, _("You will not hear game results.\n"));
  return VAR_OK;
}

static int set_helpme(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_HELPME) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_HELPME))
    pprintf(p, _("Your mistypes will be sent to the help channel.\n"));
  else
    pprintf(p, _("Your mistypes won't be sent to the help channel.\n"));
  return VAR_OK;
}

/*
static int set_private(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_PRIVATE) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_PRIVATE))
    pprintf(p, _("Your games will be private.\n"));
  else
    pprintf(p, _("Your games may not be private.\n"));
  return VAR_OK;
}
*/

static int set_bell(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_BELL) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_BELL))
    pprintf(p, _("Bell on.\n"));
  else
    pprintf(p, _("Bell off.\n"));
  return VAR_OK;
}

static int set_highlight(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 15))
    return VAR_BADVAL;

  if ((pp->highlight = v)) {
    pprintf(p, _("Highlight is now style "));
    pprintf_highlight(p, "%d", v);
    pprintf(p, ".\n");
  } else
    pprintf(p, _("Highlight is off.\n"));
  return VAR_OK;
}

static int set_style(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 1) || (v > MAX_STYLES))
    return VAR_BADVAL;
  pp->style = v - 1;
  pprintf(p, _("Style %d set.\n"), v);
  return VAR_OK;
}

static int set_flip(int p, char *var, char *val)
{
  if (set_boolean_flag(p, val, PFLAG_FLIP) < 0)
    return VAR_BADVAL;
  if (CheckPFlag(p, PFLAG_FLIP))
    pprintf(p, _("Flip on.\n"));
  else
    pprintf(p, _("Flip off.\n"));
  return VAR_OK;
}
/* Was pointless as this is what notes are for
static int set_uscf(int p, char *var, char *val)
{
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 3000))
    return VAR_BADVAL;
  pp->uscfRating = v;
  pprintf(p, "USCF Rating set to %d.\n", v);
  return VAR_OK;
}
*/
static int set_time(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 240))
    return VAR_BADVAL;
  pp->d_time = v;
  pprintf(p, _("Default time set to %d.\n"), v);
  return VAR_OK;
}

static int set_inc(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  int v = -1;

  if (!val)
    return VAR_BADVAL;
  if (sscanf(val, "%d", &v) != 1)
    return VAR_BADVAL;
  if ((v < 0) || (v > 300))
    return VAR_BADVAL;
  pp->d_inc = v;
  pprintf(p, _("Default increment set to %d.\n"), v);
  return VAR_OK;
}

/* const char *Language(int i)
{
  static const char *Lang[NUM_LANGS] = {N_("English"), N_("Spanish"), N_("French"), N_("Danish")};
  return _(Lang[i]);
}*/

 static int set_language (int p, char *var, char *val)
{
  /*  struct player *pp = &player_globals.parray[p];
  int i, len, gotIt = -1;

  if (!val)
    return VAR_BADVAL;
  len = strlen(val);
  for (i=0; i < NUM_LANGS; i++) {
    if (strncasecmp(val, Language(i), len))
      continue;
    if (gotIt >= 0)
      return VAR_BADVAL;
    else gotIt = i;
  }
  if (gotIt < 0)
    return VAR_BADVAL;
  pp->language = gotIt;
  pprintf(p, _("Language set to %s.\n"), Language(gotIt)); */
  return VAR_OK;
}

static int set_promote(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  if (!val)
    return VAR_BADVAL;
  stolower(val);
  switch (val[0]) {
  case 'q':
    pp->promote = QUEEN;
    pprintf(p, _("Promotion piece set to QUEEN.\n"));
    break;
  case 'r':
    pp->promote = ROOK;
    pprintf(p, _("Promotion piece set to ROOK.\n"));
    break;
  case 'b':
    pp->promote = BISHOP;
    pprintf(p, _("Promotion piece set to BISHOP.\n"));
    break;
  case 'n':
  case 'k':
    pp->promote = KNIGHT;
    pprintf(p, _("Promotion piece set to KNIGHT.\n"));
    break;
  default:
    return VAR_BADVAL;
  }
  return VAR_OK;
}


static int set_interface(int p, char *var, char *val)
{
	struct player *pp = &player_globals.parray[p];

	if (!val) {
		FREE(pp->interface);
		return VAR_OK;
	}
	if (!printablestring(val))
		return VAR_BADVAL;
	FREE(pp->interface);
	pp->interface = strdup(val);
	return VAR_OK;
}

static int set_prompt(int p, char *var, char *val)
{
	struct player *pp = &player_globals.parray[p];

	if (!val) {
		free(pp->prompt);
		pp->prompt = strdup(config.strs[default_prompt]);
		return VAR_OK;
	}
	if (!printablestring(val))
		return VAR_BADVAL;
	free(pp->prompt);
	pp->prompt = strdup(val);
	return VAR_OK;
}

static int RePartner (int p, int new)
{
  struct player *pp = &player_globals.parray[p];
  int pOld;

  if (p < 0)
    return -1;
  pOld = pp->partner;
  if (pOld >= 0) {
    if (player_globals.parray[pOld].partner == p) {
      if (new >= 0)
        pprintf_prompt (pOld, U_("Your partner has just chosen a new partner.\n")); /* xboard */
      else {
        pprintf (pOld, _("Your partner has unset his/her partner variable.\n"));
        pprintf_prompt (pOld, U_("You are no longer %s's partner.\n"),pp->name); /* xboard */
      }
      decline_withdraw_offers (pOld, -1, PEND_BUGHOUSE,DO_DECLINE | DO_WITHDRAW);
      player_globals.parray[pOld].partner = -1;
    }
  }
  decline_withdraw_offers(p, -1, PEND_BUGHOUSE,DO_DECLINE | DO_WITHDRAW);
  decline_withdraw_offers(p, -1, PEND_PARTNER,DO_DECLINE | DO_WITHDRAW);
  pp->partner = new;
  return new;
}

int com_partner(int p, param_list param)
{
  struct player *pp = &player_globals.parray[p];
  int pNew;
  struct pending* pend;

  if (param[0].type == TYPE_NULL) {
    if (pp->partner >= 0)
      pprintf (p, U_("You no longer have a bughouse partner.\n")); /* xboard */
    else pprintf (p, _("You do not have a bughouse partner.\n"));
    RePartner(p, -1);
    return COM_OK;
  }
  /* OK, we're trying to set a new partner. */
  pNew = player_find_part_login(param[0].val.word);
  if (pNew < 0 || player_globals.parray[pNew].status == PLAYER_PASSWORD
      || player_globals.parray[pNew].status == PLAYER_LOGIN) {
    pprintf(p, _("No user named \"%s\" is logged in.\n"), param[0].val.word);
    return COM_OK;
  }
  if (pNew == p) {
    pprintf(p, _("You can't be your own bughouse partner.\n"));
    return COM_OK;
  }
  /* Now we know a legit partner has been chosen.  Is an offer pending? */
  if ((pend = find_pend(pNew, p, PEND_PARTNER)) != NULL) {
    pprintf (p, _("You agree to be %s's partner.\n"), player_globals.parray[pNew].name);
    Bell (pNew);
    pprintf_prompt (pNew, U_("%s agrees to be your partner.\n"), pp->name); /* xboard */

    delete_pending (pend);

    /* Make the switch. */
    RePartner (p, pNew);
    RePartner (pNew, p);
    return COM_OK;
  }
  /* This is just an offer. Make sure a new partner is needed. */
  if (player_globals.parray[pNew].partner >= 0) {
    pprintf(p, _("%s already has a partner.\n"), player_globals.parray[pNew].name);
    return COM_OK;
  }
  pprintf(pNew, "\n");
  pprintf_highlight(pNew, "%s", pp->name);
  Bell (pNew);
  pprintf(pNew, U_(" offers to be your bughouse partner; ")); /* xboard */
  pprintf_prompt(pNew, _("type \"partner %s\" to accept.\n"), pp->name);
  pprintf(p, _("Making a partnership offer to %s.\n"), player_globals.parray[pNew].name);
  add_request(p, pNew, PEND_PARTNER);

  return COM_OK;
}

static int set_busy(int p, char *var, char *val)
{
  struct player *pp = &player_globals.parray[p];
  if (!val) {
    if (pp->busy != NULL) {
      free (pp->busy);
      pp->busy = NULL;
    }
    pprintf(p, _("Your \"busy\" string was cleared.\n"));
    return VAR_OK;
  }
  if ((val) && (!printablestring(val)))
    return VAR_BADVAL;
  pp->busy = strdup(val);
  pprintf(p, _("Your \"busy\" string was set to \" %s\"\n"), pp->busy);
  return VAR_OK;
}

static int set_formula(int p, char *var, char *val)
{
	int which;
	struct player *me = &player_globals.parray[p];

#ifdef NO_FORMULAS
	pprintf(p, _("Sorry -- not available because of a bug\n"));
	return COM_OK;
#else
	if (isdigit(var[1]))
		which = var[1] - '1';
	else
		which = MAX_FORMULA;

	if (val != NULL) {
		val = eatwhite(val);
		if (val[0] == '\0')
			val = NULL;
	}
	if (!SetValidFormula(p, which, val))
		return VAR_BADVAL;

	if (which < MAX_FORMULA) {
		if (val != NULL) {
			while (me->num_formula < which) {
				me->formulaLines[me->num_formula] = NULL;
				(me->num_formula)++;
			}
			if (me->num_formula <= which)
				me->num_formula = which + 1;
			pprintf(p, _("Formula variable f%d set to %s.\n"),
					which + 1, me->formulaLines[which]);
			return VAR_OK;
		}
		pprintf(p, _("Formula variable f%d unset.\n"), which + 1);
		if (which + 1 >= me->num_formula) {
			while (which >= 0 && me->formulaLines[which] == NULL)
				which--;
			me->num_formula = which + 1;
		}
	} else {
		if (me->formula != NULL)
			pprintf(p, _("Formula set to %s.\n"), me->formula);
		else
			pprintf(p, _("Formula unset.\n"));
	}
	return VAR_OK;
#endif
}

static int set_find(char *var)
{
  int i = 0, gotIt = -1;
  unsigned len = strlen(var);

  while (variables[i].name) {
    if (!strncmp(variables[i].name, var, len)) {
      if (len == strlen(variables[i].name))
		return i;
      else if (gotIt >= 0)
		return -VAR_AMBIGUOUS;
      gotIt = i;
    }
    i++;
  }
  if (gotIt >= 0)
    return gotIt;

  return -VAR_NOSUCH;
}

int var_set(int p, char *var, char *val, int *wh)
{
  int which;

  if (!var)
    return VAR_NOSUCH;
  if ((which = set_find(var)) < 0)
    return -which;

  *wh = which;
  return variables[which].var_func(p, (isdigit(*variables[which].name) ? var : variables[which].name), val);
}

int com_variables(int p, param_list param)
{
  int p1, connected;
  int i;

  if (param[0].type == TYPE_WORD) {
    if (!FindPlayer(p, param[0].val.word, &p1, &connected))
      return COM_OK;
  } else {
      p1 = p;
      connected = 1;
  }

  pprintf(p, _("Variable settings of %s:\n\n"), player_globals.parray[p1].name);

  pprintf(p, "time=%-3d    inc=%-3d       shout=%d         pin=%d           style=%-3d\n",
	  player_globals.parray[p1].d_time, player_globals.parray[p1].d_inc,
	  BoolCheckPFlag(p1, PFLAG_SHOUT), BoolCheckPFlag(p1, PFLAG_PIN),
	  player_globals.parray[p1].style + 1);

  pprintf(p, "tourney=%d   helpme=%d 	  rated=%d         gin=%d 			tell=%d\n",
	  BoolCheckPFlag(p1, PFLAG_TOURNEY), BoolCheckPFlag(p1, PFLAG_HELPME),
	  BoolCheckPFlag(p1, PFLAG_RATED), BoolCheckPFlag(p1, PFLAG_GIN),
	  BoolCheckPFlag(p1, PFLAG_TELL)
	  );

  pprintf(p, "kibitz=%d    availinfo=%d   highlight=%d     simopen=%d		open=%d\n",
	  BoolCheckPFlag(p1, PFLAG_KIBITZ), BoolCheckPFlag(p1, PFLAG_AVAIL),
	  player_globals.parray[p1].highlight, BoolCheckPFlag(p1, PFLAG_SIMOPEN),
	  BoolCheckPFlag(p1, PFLAG_OPEN)
  );

  if (player_globals.parray[p1].prompt)
    pprintf(p, "Prompt: %s\n", player_globals.parray[p1].prompt);
  else
    pprintf(p, "\n");

  if (player_globals.parray[p1].partner >= 0)
    pprintf(p, _("\nBughouse partner: %s\n"),
			player_globals.parray[player_globals.parray[p1].partner].name);

  if (player_globals.parray[p1].num_formula) {
    pprintf(p, "\n");
    for (i = 0; i < player_globals.parray[p1].num_formula; i++) {
      if (player_globals.parray[p1].formulaLines[i] != NULL)
        pprintf(p, " f%d: %s\n", i + 1, player_globals.parray[p1].formulaLines[i]);
      else
        pprintf(p, " f%d:\n", i + 1);
    }
  }
  if (player_globals.parray[p1].formula != NULL)
    pprintf(p, _("\nFormula: %s\n"), player_globals.parray[p1].formula);
  else
    pprintf(p, "\n");



#if 0
  	// gabrielsan: obsolete variable removed
  pprintf(p, "jprivate=%d    cshout=%d        notifiedby=%d    flip=%d\n",
	  BoolCheckPFlag(p1, PFLAG_JPRIVATE),
	  BoolCheckPFlag(p1, PFLAG_CSHOUT), BoolCheckPFlag(p1, PFLAG_NOTIFYBY),
	  BoolCheckPFlag(p1, PFLAG_FLIP));
#endif
#if 0
  	// gabrielsan: obsolete variable removed
  pprintf(p, "automail=%d    kiblevel=%-4d   availmin=%-4d   bell=%d\n",
	  BoolCheckPFlag(p1, PFLAG_AUTOMAIL),
	  player_globals.parray[p1].kiblevel, player_globals.parray[p1].availmin,
	  BoolCheckPFlag(p1, PFLAG_BELL));

  	// gabrielsan: obsolete variable removed
  pprintf(p, "ropen=%d     pgn=%d         tell=%d          availmax=%-4d   width=%-3d\n",
	  BoolCheckPFlag(p1, PFLAG_ROPEN), BoolCheckPFlag(p1, PFLAG_PGN),
          BoolCheckPFlag(p1, PFLAG_TELL), player_globals.parray[p1].availmax,
          player_globals.parray[p1].d_width);

  	// gabrielsan: obsolete variable removed
  pprintf(p, "mailmess=%d  height=%-3d\n",
	  BoolCheckPFlag(p1, PFLAG_MAILMESS), player_globals.parray[p1].d_height);
#endif

  if (!connected)
    player_remove(p1);
  return COM_OK;
}

/* tournset (cause settourney is BAD for set)
 *   used by Tournament Director programs to set/unset the
 *   tourney variable of another registered player.
 * fics usage: tournset playername int   (int can be 0 or 1)
 */
int com_tournset(int p, param_list param)
{
  struct player *pp = &player_globals.parray[p];
  int p1, v;

  if (!in_list(p, L_TD, pp->name)) {
    pprintf(p, _("Only TD programs are allowed to use this command.\n"));
    return COM_OK;
  }
  if (((p1 = player_find_bylogin(param[0].val.word)) < 0)
      || (!CheckPFlag(p1, PFLAG_REG))) {
     return COM_OK;
  }
  v = BoolCheckPFlag(p1, PFLAG_TOURNEY);
  if (param[1].val.integer == 0) {  /* set it to 0 */
    if (v == 1) {
      SetPFlag(p1, PFLAG_TOURNEY, 0);
      pprintf_prompt(p1, _("\n%s has set your tourney variable to OFF.\n"),
            pp->name);
    }
  } else {  /* set it to 1 */
    if (v == 0) {
      SetPFlag(p1, PFLAG_TOURNEY, 1);
      pprintf_prompt(p1, _("\n%s has set your tourney variable to ON.\n"),
            pp->name);
    }
  }
  return COM_OK;
}


