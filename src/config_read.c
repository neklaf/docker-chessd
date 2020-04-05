#define MAX_LINE_LENGTH	512

#include "config_read.h"

char* trim(char *str)
{
	int ini, fin;
	char *stemp;

	fin = strlen(str);
	for (fin--; fin > 0 && isspace(str[fin]); fin--)
		;
	for (ini = 0; ini < fin && isspace(str[ini]); ini++)
		;

	str[++fin] = 0;
	stemp = &str[ini];
	memmove(str, stemp, fin-ini+1);

	return str;
}

void strip_comments(char* line)
{
	int i, len;
	int quote=0;

	len = strlen(line);
	for (i = 0; i < len && (line[i] != '#' || quote); i++) {
		if (line[i] == '"') {
			quote ^= 1;
			line[i] = ' ';
		}
	}
	line[i] = 0;
}

int read_config(char* filename, config_key** result)
{
	FILE *fp;
	config_key* conf_data;
	char line[MAX_LINE_LENGTH+1];
	int i, eq_mark, cline, conf_pos, c;

	*result = NULL;
	fp = fopen(filename, "r");
	if (fp == NULL)
		return -1;

	// just count the number of lines to find out how much we should allocate
	cline = 0;
	conf_pos = 0;
	while ((c = fgetc(fp)) != EOF) {
		// increment counter for each line that contains '='
		if (c == '=')
			conf_pos = 1;
		else if (c == '\n' && conf_pos) {
			cline++;
			conf_pos = 0;
		}
	}
	cline += conf_pos; // in case the last line has no newline
	if (cline == 0)
		return -1;

	conf_data = malloc(sizeof(config_key) * cline);

	cline = 0;
	conf_pos = 0;
	rewind(fp);
	while (fgets(line, MAX_LINE_LENGTH, fp)) {
		strip_comments(line);

		// Find '=', and end of line. Stop at newline, end of string, or buffer end.
		for (i = eq_mark = 0; i < MAX_LINE_LENGTH && line[i] != '\n' && line[i]; i++)
			if(line[i] == '=' && !eq_mark && i)
				eq_mark = i;

		if (eq_mark) // only considers valid lines
		{
			line[eq_mark++] = 0;
			line[i] = 0;
			conf_data[conf_pos].name = trim(strdup(line));
			conf_data[conf_pos].value = trim(strdup(&line[eq_mark]));
			conf_pos++;
		}
		cline++;
	}

	*result = conf_data;

	return conf_pos;
}

