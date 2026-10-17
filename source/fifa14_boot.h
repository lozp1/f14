/* fifa14_boot.h -- FIFA 14's boot: the JNI environment, the natives bound by
 * symbol, the engine's own start-up order, and the frame loop (fifa14_boot.c).
 *
 * port_run() (fifa14_main.c) is the caller: fifa14_boot_bind() once, then
 * fifa14_boot_run() and it does not return until the game, the system or the
 * port asks to stop.
 *
 * MIT.
 */
#ifndef FIFA14_BOOT_H
#define FIFA14_BOOT_H

/* Bind every name in config.h's entry-point list to its address in
 * libFIFA14.so (so_try_find_addr_rx) and log the result of each: found, with
 * its address, or NOT FOUND. Safe to call twice. */
void fifa14_boot_bind(void);

/* The address of a bound entry point (0: not found). fifa14_blast_stubs.c
 * uses it to hand the engine's own EAIO/audio natives their calls. */
void *fifa14_native_addr(const char *symbol);

/* The whole start-up, then the frame loop. */
void fifa14_boot_run(void);

#endif /* FIFA14_BOOT_H */
