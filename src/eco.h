#ifndef _ECO_H
#define _ECO_H

#include "board.h"
#include "command.h"

void FEN_to_board(char* FENpos, struct game_state_t* gs);
char *boardToFEN(int g);
void book_close(void);
void book_open(void);
char *getECO(int g);
int com_eco(int p, param_list param);

#endif
