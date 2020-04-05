/*
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

#ifndef _MALLOC_H
#define _MALLOC_H

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef USE_CHESSD_MALLOC
#undef malloc
#undef free
#undef calloc
#undef strdup
#undef strndup
#undef vasprintf
#undef asprintf
#undef realloc

#define malloc m_malloc
#define calloc m_calloc
#define strdup m_strdup
#define strndup m_strndup
#define vasprintf m_vasprintf
#define asprintf m_asprintf
#define realloc m_realloc

#define free(x) m_safe_free((void **)&(x), __FILE__, __LINE__)

#endif
#define FREE(x) ((x)?free(x),(x)=NULL:0)

void m_malloc_check(void *ptr);
int m_free(void *ptr);

int s_free(void *ptr);
void m_check_all(void);
void m_free_all(void);
void *m_calloc(size_t n, size_t size);
char *m_strdup(const char *s);
char *m_strndup(const char *s, size_t n);
void m_safe_free(void **ptr, const char *file, unsigned line);
int m_vasprintf(char **strp, const char *fmt, va_list ap);
int m_asprintf(char **strp, const char *fmt, ...);
void *m_realloc(void *ptr, size_t size);

void *m_malloc(size_t size);

#endif /* _MALLOC_H */
