/* Local stub of <miniz/miniz.h>, for the HOST syntax check only
 * (tools/syntax-check.sh). The real miniz lives only in the build container's
 * image, so without this a host -fsyntax-only of fifa14_blast_stubs.c dies on
 * the include before it can report anything useful.
 *
 * This file is NOT on the container build's include path (runtime/runtime.mk
 * does not know about tools/), so it can never shadow the real header. */
#ifndef FIFA14_HOST_MINIZ_STUB_H
#define FIFA14_HOST_MINIZ_STUB_H

#include <stddef.h>

size_t tinfl_decompress_mem_to_mem(void *out, size_t out_len, const void *src, size_t src_len,
                                   int flags);
#define TINFL_FLAG_PARSE_ZLIB_HEADER 1

/* dcr_setup.h forward-declares it in its signatures; the real miniz defines the
 * struct. Opaque here is enough for -fsyntax-only: nothing in the port's own
 * sources dereferences it (only the runtime's dcr_setup.c does, and that one is
 * not in this host check's file list). */
typedef struct mz_zip_archive mz_zip_archive;

#endif /* FIFA14_HOST_MINIZ_STUB_H */
