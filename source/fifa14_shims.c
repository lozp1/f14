/* fifa14_shims.c -- the five bionic shims FIFA 14 imports that the android32
 * runtime's own bionic_*.c does not provide.
 *
 * gen_imports.py reports these 12 leftovers of the port's import set (run with
 * --libs); seven of them are config, not code, and live in tools/imports.cfg:
 *
 *   PASSTHROUGH  isnan atoll wcscmp wcsncpy wcsstr wcstombs
 *                (ABI-compatible: devkitARM's newlib wchar_t is 4 bytes, as
 *                 bionic's, and libc.a exports all six)
 *   DATA         tzname _tzname        (newlib's char*[2], same layout)
 *
 * The five below have no equivalent in newlib (or take a bionic FILE ptr or a
 * struct), so they cannot be a PASSTHROUGH entry:
 *
 *   __isinf                        bionic's libm helper; newlib's libm exports
 *                                  isinf and __fpclassifyd, never __isinf.
 *   fwide                          takes a FILE*: bionic's 84-byte __sF entry
 *                                  or a newlib FILE* (bionic_stdio.c), so the
 *                                  stream is mapped before newlib sees it.
 *   pthread_attr_setschedpolicy    bionic-only; recorded in the runtime's
 *   pthread_attr_setstack          b_pthread_attr_t (bionic.h) so a matching
 *   pthread_mutexattr_setpshared   getter reads back what was set.
 *
 * Load order / ABI match: source/config.h. MIT.
 */
#include <stdio.h>
#include <stdlib.h> /* wcstombs() */
#include <string.h>
#include <wchar.h>

#include "bionic.h"
#include "util.h"

/* ---------------------------------------------------------------- wide paths */

/* wcstombs() is the ONLY wide (UTF-32) -> char conversion libFIFA14.so has
 * (its import list: wcscmp/wcslen/wcsncpy/wcsstr/wcstombs/wcsxfrm -- no
 * mbstowcs, no wcsrtombs). The engine's data opens start from a wchar_t path
 * (EA::IO::AssetStream::CreateMemoryStreamFromFile(const wchar_t*), the frame
 * the data abort came from), so the converted char string IS the path that
 * then reaches open()/stat(). Logging both here is what makes the wide path
 * visible: the engine's own conversion leaves no other trace.
 *
 *   [fs] WIDE    wide="/data/data/com.ea.game.fifa14_row/files/assets/adview.html" -> char="assets/adview.html" (n=4096 rc=18)
 *
 * Only strings that look like a path (a '/' or a ':') are logged, budgeted at
 * 256 lines; non-ASCII wide chars show as '?'. */
size_t b_wcstombs(char *dst, const wchar_t *src, size_t n) {
  const size_t r = wcstombs(dst, src, n);
  static int logged;
  if (src && logged < 256) {
    char wide[192];
    size_t wi = 0;
    for (; wi + 1 < sizeof wide && src[wi]; wi++)
      wide[wi] = (src[wi] > 31 && src[wi] < 127) ? (char)src[wi] : '?';
    wide[wi] = 0;
    if (strchr(wide, '/') || strchr(wide, ':')) {
      char narrow[160];
      narrow[0] = 0;
      /* Trust the result only when it fits: wcstombs() reports failure with
       * (size_t)-1, and a run that exactly fills n wrote no terminator. */
      if (dst && r != (size_t)-1 && r < n && r < sizeof narrow - 1) {
        memcpy(narrow, dst, r);
        narrow[r] = 0;
      }
      logged++;
      debugPrintf("[fs] WIDE    wide=\"%s\" -> char=\"%s\" (n=%u rc=%d)\n", wide, narrow,
                  (unsigned)n, r == (size_t)-1 ? -1 : (int)r);
    }
  }
  return r;
}

/* ------------------------------------------------------------------- libm */

/* bionic's __isinf: 1 for +Inf, -1 for -Inf, 0 otherwise (BSD semantics; the
 * game's compiled isinf() calls it). */
int b___isinf(double x) {
  if (x == (double)__builtin_inf())
    return 1;
  if (x == -(double)__builtin_inf())
    return -1;
  return 0;
}

/* ------------------------------------------------------------------ stdio */

/* fwide(stream, mode): a stream the game holds is either one of the three
 * 84-byte bionic __sF entries -- map it to newlib's stdin/stdout/stderr, the
 * same remap bionic_stdio.c's real_fp() does -- or a newlib FILE* b_fopen()
 * returned, which newlib's fwide takes directly. */
int b_fwide(void *stream, int mode) {
  unsigned char *p = stream;
  FILE *fp = (FILE *)stream;
  if (p == &b___sF[0])
    fp = stdin;
  else if (p == &b___sF[B_FILE_SIZE])
    fp = stdout;
  else if (p == &b___sF[2 * B_FILE_SIZE])
    fp = stderr;
  return fwide(fp, mode);
}

/* ---------------------------------------------------------------- pthread */

/* Horizon schedules the guest threads itself (dcr_sched.c); the policy is only
 * recorded so b_pthread_attr_getschedpolicy() (bionic_pthread.c) reads back
 * what was set. */
int b_pthread_attr_setschedpolicy(b_pthread_attr_t *a, int policy) {
  if (!a)
    return L_EINVAL;
  a->sched_policy = policy;
  return 0;
}

/* pthread_attr_setstack(): the runtime's attribute carries stack_base and
 * stack_size (bionic.h), the same fields b_pthread_attr_getstack() reports.
 * b_pthread_create_on() reads only stack_size (the host stack is the runtime's),
 * so a guest stack_base stored here is harmless. The size is validated as
 * b_pthread_attr_setstacksize() validates it. */
int b_pthread_attr_setstack(b_pthread_attr_t *a, void *base, size_t size) {
  if (!a || size < 0x2000) /* bionic's PTHREAD_STACK_MIN */
    return L_EINVAL;
  a->stack_base = base;
  a->stack_size = size;
  return 0;
}

/* The runtime's mutexattr is one long word whose low bits are the mutex type
 * (bionic_pthread.c, "*a & 3"); process-shared is a separate bit no mutex here
 * consults (Horizon has one address space), set/cleared without touching the
 * type. PTHREAD_PROCESS_PRIVATE=0, PTHREAD_PROCESS_SHARED=1. */
int b_pthread_mutexattr_setpshared(b_pthread_mutexattr_t *a, int pshared) {
  if (!a || (pshared != 0 && pshared != 1))
    return L_EINVAL;
  if (pshared)
    *a |= 0x10L;
  else
    *a &= ~0x10L;
  return 0;
}
