/*
  Copyright (c) 1999 Mark Murray.
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

#include <string.h>
#include "crypt-md5.h"

char *chessd_crypt(const char *passwd, const char *salt)
{
	if (!strncmp(salt, "$1$", 3)) {
		salt += 3;
		salt = salt + strlen(salt) - strcspn(salt, "$");
	}
	return crypt_md5(passwd, salt);
}
