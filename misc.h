/*
 * AOPPA -- A program for extracting the AnarchyOnline resource database
 * Prototypes for misc.cc.
 *
 * $Id: misc.h 1111 2009-03-04 07:57:14Z os $
 *
 * Copyright (C) 2002-2009 Oskari Saarenmaa <auno@auno.org>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _AOPPA_MISC_H
#define _AOPPA_MISC_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(_GNU_SOURCE) && !defined(__linux__)
#undef _GNU_SOURCE
#endif /* _GNU_SOURCE && !__linux__ */

#if defined(_GNU_SOURCE) || defined(__NetBSD__)
#define HAVE_ASPRINTF
#endif /* _GNU_SOURCE || __NetBSD__ */

#if defined(_GNU_SOURCE)
#define HAVE_STRNDUP
#define HAVE_MEMMEM
#endif /* _GNU_SOURCE */

unsigned int aoppa_crc32(const unsigned char *, size_t);

#define DEBUG_SERIOUS  0
#define DEBUG_INFO     4
#define DEBUG_VERBOSE  6
extern int g_debug;
void   debugf(int, const char *, ...);

#ifndef HAVE_MEMMEM
void * memmem(const void *, size_t, const void *, size_t);
#endif /* !HAVE_MEMMEM */

#ifndef HAVE_STRNDUP
char * strndup(const char *, unsigned int);
#endif /* !HAVE_STRNDUP */

#ifndef HAVE_ASPRINTF
int    asprintf(char **, const char *, ...);
#endif /* !HAVE_ASPRINTF */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_AOPPA_MISC_H */
