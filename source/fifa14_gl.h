/* fifa14_gl.h -- the GL surface the phone's Java would have made
 * (fifa14_gl.c).
 *
 * On Android the GL context is NOT the game's: the activity's Java
 * (GLSurfaceView) creates the EGL display/config/context/ window surface and
 * makes it current on its GL thread; the native side only finds it through
 * eglGetCurrentDisplay()/eglGetCurrentSurface() and presents through
 * eglSwapBuffers(). libFIFA14.so imports exactly those three and NO
 * eglCreateContext/eglMakeCurrent (not as an import line, and not as a string
 * for a dlsym either -- checked in .dynsym and in .rodata), so with no Java
 * there is no context at all unless the port makes one.
 *
 * fifa14_gl_boot() is that Java step, done before the engine's init frame.
 * fifa14_gl_renderer() is the renderer string the engine read its GPU table
 * with (the port's own answer when the GL has none).
 *
 * MIT.
 */
#ifndef FIFA14_GL_H
#define FIFA14_GL_H

/* Create the EGL display/config/GLES2 context/window surface on the runtime's
 * window and make it current. Idempotent. Every failure is logged; nothing
 * here is fatal (the engine is then answered by the port's stand-in strings). */
void fifa14_gl_boot(void);

/* What glGetString(GL_RENDERER) answers now ("" before fifa14_gl_boot). */
const char *fifa14_gl_renderer(void);

/* Is the port's context the current one? (The engine's glGetString needs it:
 * Mesa's glGetString returns NULL with no current context -- the crash of
 * Blast::System::Init, see fifa14_gl.c.) */
int fifa14_gl_current(void);

/* THE PRESENT. Present the back buffer the port's own context has been drawing
 * into, the way Android's GLSurfaceView does after the Renderer's onDrawFrame:
 * its GLThread's EGLHelper.swap() -> EGL.eglSwapBuffers(display, surface). The
 * engine NEVER calls eglSwapBuffers itself (measured: ~4500 frames at 30 fps
 * with "engine swaps: 0" and no "[egl] eglSwapBuffers" line), because on the
 * phone the swap is the Java layer's, not the engine's -- the engine only
 * paints the back buffer. So the port's frame loop calls this after every
 * engine frame, exactly as the GLSurfaceView's render thread does.
 *
 * It goes through the runtime's own eglSwapBuffers (dcr_gl_lookup), so the
 * present hook, the capture and dcr_gl_frames() (the [loop] "engine swaps"
 * counter and the watchdog's) all count here. Returns 1 when the frame was
 * handed to the display, 0 when there was nothing to swap (no current
 * context). The first few swaps and then one every 300 are logged:
 *   [gl] swap #1: eglSwapBuffers(0x..., 0x...) -> 1 err 0x0 */
int fifa14_gl_swap(void);

/* DID THE ENGINE PAINT? Count, and then log and reset, the engine's GL calls
 * that mean drawing: glClear, glDrawArrays/glDrawElements, glTexImage2D/
 * glTexSubImage2D, glUseProgram, glViewport, glBlendFunc. One line per window
 * of `frames` engine frames (no flood). A swap of an empty back buffer is a
 * black screen too, so this separates "the engine draws and nothing presents
 * it" (the bug this file fixes) from "the engine draws nothing at all" (a
 * different wall: its renderer/Java side never starts). The counters come from
 * port_gl_wrap, i.e. every gl* the engine LOOKS UP (its imports, dlsym and
 * eglGetProcAddress all resolve through dcr_gl_lookup), so a 0 here is the
 * engine's own, not a missed hook. */
void fifa14_gl_draw_window(unsigned frames);

#endif /* FIFA14_GL_H */
