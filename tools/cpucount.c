/***********************************************************************
 *  OSXCross                                                           *
 *  Copyright (C) 2013, 2014 by Thomas Poechtrager                     *
 *  t.poechtrager@gmail.com                                            *
 *                                                                     *
 *  This program is free software; you can redistribute it and/or      *
 *  modify it under the terms of the GNU General Public License        *
 *  as published by the Free Software Foundation; either version 2     *
 *  of the License, or (at your option) any later version.             *
 *                                                                     *
 *  This program is distributed in the hope that it will be useful,    *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
 *  GNU General Public License for more details.                       *
 *                                                                     *
 *  You should have received a copy of the GNU General Public License  *
 *  along with this program; if not, write to the Free Software        *
 *  Foundation, Inc.,                                                  *
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.      *
 ***********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#ifdef __CYGWIN__
#define WIN32
#endif /* __CYGWIN__ */

#ifdef WIN32
#include <windows.h>
#endif /* WIN32 */

#ifdef __linux__
#define __USE_GNU
#include <sched.h>
#undef __USE_GNU
#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#ifndef HW_AVAILCPU
#define HW_AVAILCPU 25
#endif /* HW_AVAILCPU */
#endif /* BSD */

int getcpucount() {
#ifdef WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);

  return sysinfo.dwNumberOfProcessors;
#else
#ifdef __linux__
  cpu_set_t cs;
  int i, cpucount = 0;

  CPU_ZERO(&cs);
  sched_getaffinity(0, sizeof(cs), &cs);

  for (i = 0; i < 128; i++) {
    if (CPU_ISSET(i, &cs))
      cpucount++;
  }

  return cpucount ? cpucount : 1;
#else
#if defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__APPLE__)
  int cpucount = 0;
  int mib[4];
  size_t len = sizeof(cpucount);

  mib[0] = CTL_HW;
  mib[1] = HW_AVAILCPU;

  sysctl(mib, 2, &cpucount, &len, NULL, 0);

  if (cpucount < 1) {
    mib[1] = HW_NCPU;
    sysctl(mib, 2, &cpucount, &len, NULL, 0);
  }

  return cpucount ? cpucount : 1;
#else
#warning unknown platform
  return 1;
#endif /* BSD */
#endif /* __linux__ */
#endif /* WIN32 */
}

int main(void) {
  printf("%d\n", getcpucount());
  return 0;
}
