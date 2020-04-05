#ifndef _ALGCHECK_H
#define _ALGCHECK_H

#include "board.h"

int alg_is_move(const char *mstr);
int alg_parse_move(char *mstr, struct game_state_t * gs, struct move_t * mt);
char *alg_unparse(struct game_state_t * gs, struct move_t * mt);

#endif
