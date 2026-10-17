/* fifa14_gl.c -- the GL surface the phone's Java would have made, and the
 * answer to the draw-abort that stopped Blast::System::Init.
 *
 * THE CRASH, AND WHY IT LANDS HERE. crash.log's last one is a data abort at
 * libFIFA14.so+0xe5c8b8, `ldrb r3,[r0]` with r0 = 0 and r2 = 0x4d, i.e. inside
 * EA::StdC::Strcmp(libFIFA14.so+0xe5c8b4):
 *
 *   pc  3925b8b8  +0xe5c8b8  EA::StdC::Strcmp+0x4       (base 0x38400000)
 *   lr  3896519c  +0x56619c  the caller's return address
 *   r1  394086ef             the string compared against
 *
 * The caller is the function at +0x566038 (its only `bl` to Strcmp, at
 * +0x566198), and r1/r8 there are the PIC pair: r8 = pc + 0xb81780 (the
 * instruction at +0x566050, `add r8, pc, r0`) = 0x394e77d8, so
 * r1 = r8 + 0xfff21f17 = 0x394096ef -> .rodata+0x010096ef = **"Mali-T604"**,
 * which is 0x4d = 'M' = the r2 in the dump. The NULL compared against it is the
 * return of the virtual call at +0x566184:
 *
 *   r0 = *(GOT slot +0xe1ea4)        -> the graphics object (EA::Graphics::...)
 *   r1 = *r0; r2 = *[r1 + 0x124]     -> vtable slot 0x124
 *   r2(r0, 0x1F01)                   -> returned NULL
 *
 * 0x1F01 is GL_RENDERER, and the slot is
 * EA::Graphics::OpenGLES20Managed::glGetString (+0xe1f9f0: forwards to the
 * +8 thunk, OpenGLES20::glGetString +0xe1821c, `bl glGetString@plt`).
 * The caller is the engine's GPU table (the strings next to it: "Mali-T604",
 * "Adreno (TM) 200/205/220", "PowerVR SGX 530/535/540/543/544", "NVIDIA Tegra",
 * "GT-I8190N"), read during Blast::System::Init -> ModuleManager::Init ->
 * InitAutoInstantiatedModules -> GetModule -> the module's Initialize.
 *
 * WHY IT WAS NULL. glGetString itself returned 0, and the two ways the port
 * can make that happen pin each other down:
 *   - DCR_GL_MESA=0 (the null renderer) cannot: gl_null.c's n_glGetString
 *     answers 0x1F01 with "null renderer". The build is the mesa one
 *     (build/.renderer-1-mesa20).
 *   - Mesa's glGetString is `GET_CURRENT_CONTEXT(ctx); if (!ctx) return NULL;`
 *     -- with NO CURRENT CONTEXT it returns NULL. The port creates no context
 *     anywhere (nothing in source/ calls eglInitialize/eglCreateContext/
 *     eglMakeCurrent), and it cannot rely on the game: libFIFA14.so imports
 *     only eglGetCurrentDisplay/eglGetCurrentSurface/eglSwapBuffers -- on
 *     Android the Java GLSurfaceView owns the context and the native side just
 *     finds it. That is the crash: the engine asked the GL for its renderer
 *     before any context existed.
 *
 * THE FIX. fifa14_gl_boot() is the Java GLSurfaceView step: display + config +
 * GLES2 context + window surface on the runtime's window, made current on this
 * thread, BEFORE the engine's init frame (fifa14_boot.c step 7b). The entry
 * points come from dcr_gl_lookup, so the file is renderer-agnostic (both
 * gl_mesa.c and gl_null.c serve the EGL names) and needs no EGL headers.
 *
 * Belt to those braces: port_gl_wrap returns a wrapper for "glGetString" that
 * can never hand the engine a NULL -- if the GL has no string (no current
 * context, or Mesa has no such entry point at all) it says so ONCE per pname
 * and answers a stand-in. Losing a frame to a wrong GPU name is free; losing
 * the boot to a strcmp on NULL is not.
 *
 * MIT.
 */
#include <string.h>
#include <switch.h>

#include "config.h"
#include "fifa14_gl.h"
#include "gl_layer.h"
#include "rt_window.h"
#include "util.h"

/* ------------------------------------------------------------------- EGL
 * The names the runtime serves through dcr_gl_lookup (gl_mesa.c's g_egl[] /
 * gl_null.c's G() list). Only the constants and the five prototypes are here:
 * the runtime already links the EGL library, and going through dcr_gl_lookup
 * is what keeps this file good with either renderer. */
#define EGL_DEFAULT_DISPLAY 0
#define EGL_NONE            0x3038
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT  0x0004
#define EGL_ALPHA_SIZE      0x3021
#define EGL_BLUE_SIZE       0x3022
#define EGL_GREEN_SIZE      0x3023
#define EGL_RED_SIZE        0x3024
#define EGL_DEPTH_SIZE      0x3025
#define EGL_STENCIL_SIZE    0x3026
#define EGL_SURFACE_TYPE    0x3033
#define EGL_WINDOW_BIT      0x0004
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_OPENGL_ES_API   0x30A0
#define EGL_VENDOR          0x3053
#define EGL_VERSION         0x3054
#define EGL_NO_CONTEXT      0
#define EGL_NO_SURFACE      0

#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_EXTENSIONS               0x1F03
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C

typedef void *(*FIFA14_EGL_GetDisplay)(void *);
typedef unsigned (*FIFA14_EGL_Initialize)(void *, int *, int *);
typedef const char *(*FIFA14_EGL_QueryString)(void *, int);
typedef unsigned (*FIFA14_EGL_ChooseConfig)(void *, const int *, void **, int, int *);
typedef void *(*FIFA14_EGL_CreateWindowSurface)(void *, void *, void *, const int *);
typedef void *(*FIFA14_EGL_CreateContext)(void *, void *, void *, const int *);
typedef unsigned (*FIFA14_EGL_MakeCurrent)(void *, void *, void *, void *);
typedef unsigned (*FIFA14_EGL_BindAPI)(unsigned);
typedef const char *(*FIFA14_GL_GetString)(unsigned);
typedef unsigned (*FIFA14_EGL_SwapBuffers)(void *, void *);
typedef unsigned (*FIFA14_EGL_GetError)(void);

static void *g_display, *g_surface, *g_context;
static int g_booted, g_current;
static uintptr_t g_real_getstring;

/* What the engine's GPU table is answered when the GL has no string. The names
 * are deliberately none of the ones the table knows ("Mali-*", "Adreno*",
 * "PowerVR*", "NVIDIA Tegra", "GT-I8190N"): an unknown renderer takes the
 * engine's default workaround path instead of a phone's. */
static const char *fifa14_gl_standin(unsigned name) {
  switch (name) {
  case GL_VENDOR: return PORT_NAME " (dcr, no GL vendor)";
  case GL_RENDERER: return PORT_NAME " (dcr, no GL renderer)";
  case GL_VERSION: return "OpenGL ES 2.0 dcr";
  case GL_EXTENSIONS: return ""; /* nothing advertised rather than a wrong list */
  case GL_SHADING_LANGUAGE_VERSION: return "OpenGL ES GLSL ES 1.00";
  default: return "";
  }
}

/* The engine's glGetString, through the import the port bound: the real one
 * when there is a string, a stand-in (logged once per pname) when there is
 * not. This is what turns "Strcmp(NULL, \"Mali-T604\")" back into a frame. */
static const char *fifa14_gl_GetString(unsigned name) {
  const char *s = NULL;
  if (g_real_getstring)
    s = ((FIFA14_GL_GetString)g_real_getstring)(name);
  if (s)
    return s;
  {
    static unsigned warned[8];
    static int nwarned;
    int known = 0;
    for (int i = 0; i < nwarned; i++)
      known |= warned[i] == name;
    if (!known && nwarned < (int)(sizeof warned / sizeof warned[0])) {
      warned[nwarned++] = name;
      debugPrintf("[gl] glGetString(0x%x) -> NULL%s: answering \"%s\" instead "
                  "(the engine strcmp()s this: a NULL here is the draw abort at "
                  "libFIFA14.so+0xe5c8b8, Strcmp(NULL, \"Mali-T604\"))\n",
                  name,
                  g_real_getstring ? (g_current ? "" : " (no current EGL context)")
                                   : " (no entry point)",
                  fifa14_gl_standin(name));
    }
  }
  return fifa14_gl_standin(name);
}

/* ------------------------------------------------- is the engine painting?
 * glGetString is wrapped for the boot; these are wrapped to ANSWER A DIFFERENT
 * QUESTION: the loop runs ~4500 frames while the screen stays black, and a
 * swap of a back buffer nobody drew into is black too. So count the calls that
 * mean real painting -- one increment per call, then the real function, so the
 * wrap is transparent. Every gl* the engine looks up resolves through
 * port_gl_wrap (dcr_gl_lookup), imports, dlsym and eglGetProcAddress alike, so
 * these see the engine's own calls whatever way it reaches them.
 *
 * The count is read and reset once per 300-frame window (fifa14_gl_draw_window,
 * called by the loop); a window with glClear and draws says "the engine paints
 * and only the present was missing", a window with nothing says "the engine
 * paints nothing yet" -- a different wall, named in the log instead of guessed.
 * glTexImage2D and glTexSubImage2D share one counter; glDrawArrays and
 * glDrawElements too (both are draws). */
typedef void (*FIFA14_GL_Clear)(unsigned);
typedef void (*FIFA14_GL_DrawArrays)(unsigned, int, int);
typedef void (*FIFA14_GL_DrawElements)(unsigned, int, unsigned, const void *);
typedef void (*FIFA14_GL_TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned,
                                     const void *);
typedef void (*FIFA14_GL_TexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned,
                                        const void *);
typedef void (*FIFA14_GL_UseProgram)(unsigned);
typedef void (*FIFA14_GL_Viewport)(int, int, int, int);
typedef void (*FIFA14_GL_BlendFunc)(unsigned, unsigned);

static uint32_t g_n_clear, g_n_draw, g_n_tex, g_n_program, g_n_viewport, g_n_blend;
static uintptr_t g_r_clear, g_r_drawarrays, g_r_drawelements, g_r_teximage, g_r_texsubimage,
    g_r_useprogram, g_r_viewport, g_r_blendfunc;

static void f14_glClear(unsigned mask) {
  g_n_clear++;
  if (g_r_clear)
    ((FIFA14_GL_Clear)g_r_clear)(mask);
}
static void f14_glDrawArrays(unsigned mode, int first, int count) {
  g_n_draw++;
  if (g_r_drawarrays)
    ((FIFA14_GL_DrawArrays)g_r_drawarrays)(mode, first, count);
}
static void f14_glDrawElements(unsigned mode, int count, unsigned type, const void *indices) {
  g_n_draw++;
  if (g_r_drawelements)
    ((FIFA14_GL_DrawElements)g_r_drawelements)(mode, count, type, indices);
}
static void f14_glTexImage2D(unsigned target, int level, int ifmt, int w, int h, int border,
                             unsigned fmt, unsigned type, const void *px) {
  g_n_tex++;
  if (g_r_teximage)
    ((FIFA14_GL_TexImage2D)g_r_teximage)(target, level, ifmt, w, h, border, fmt, type, px);
}
static void f14_glTexSubImage2D(unsigned target, int level, int x, int y, int w, int h, unsigned fmt,
                                unsigned type, const void *px) {
  g_n_tex++;
  if (g_r_texsubimage)
    ((FIFA14_GL_TexSubImage2D)g_r_texsubimage)(target, level, x, y, w, h, fmt, type, px);
}
static void f14_glUseProgram(unsigned prog) {
  g_n_program++;
  if (g_r_useprogram)
    ((FIFA14_GL_UseProgram)g_r_useprogram)(prog);
}
static void f14_glViewport(int x, int y, int w, int h) {
  g_n_viewport++;
  if (g_r_viewport)
    ((FIFA14_GL_Viewport)g_r_viewport)(x, y, w, h);
}
static void f14_glBlendFunc(unsigned sfactor, unsigned dfactor) {
  g_n_blend++;
  if (g_r_blendfunc)
    ((FIFA14_GL_BlendFunc)g_r_blendfunc)(sfactor, dfactor);
}

void fifa14_gl_draw_window(unsigned frames) {
  const int painted = g_n_clear || g_n_draw || g_n_tex;
  debugPrintf("[gl] draw window (%u frames): glClear %u, glDrawArrays/Elements %u, "
              "glTex(Sub)Image2D %u, glUseProgram %u, glViewport %u, glBlendFunc %u -- %s\n",
              frames, (unsigned)g_n_clear, (unsigned)g_n_draw, (unsigned)g_n_tex,
              (unsigned)g_n_program, (unsigned)g_n_viewport, (unsigned)g_n_blend,
              painted ? "the engine PAINTS: with the swap done, this reaches the screen"
                      : "the engine drew NOTHING: a swap of an empty back buffer is black too");
  g_n_clear = g_n_draw = g_n_tex = g_n_program = g_n_viewport = g_n_blend = 0;
}

/* gl_layer.h's callback: every gl* the engine looks up comes through here.
 * glGetString is wrapped for the boot, the draw calls for the counters above;
 * everything else is left as the runtime bound it. */
uintptr_t port_gl_wrap(const char *name, uintptr_t real) {
  if (!name)
    return 0;
  if (!strcmp(name, "glGetString")) {
    g_real_getstring = real;
    return (uintptr_t)fifa14_gl_GetString;
  }
  if (!strcmp(name, "glClear")) {
    g_r_clear = real;
    return (uintptr_t)f14_glClear;
  }
  if (!strcmp(name, "glDrawArrays")) {
    g_r_drawarrays = real;
    return (uintptr_t)f14_glDrawArrays;
  }
  if (!strcmp(name, "glDrawElements")) {
    g_r_drawelements = real;
    return (uintptr_t)f14_glDrawElements;
  }
  if (!strcmp(name, "glTexImage2D")) {
    g_r_teximage = real;
    return (uintptr_t)f14_glTexImage2D;
  }
  if (!strcmp(name, "glTexSubImage2D")) {
    g_r_texsubimage = real;
    return (uintptr_t)f14_glTexSubImage2D;
  }
  if (!strcmp(name, "glUseProgram")) {
    g_r_useprogram = real;
    return (uintptr_t)f14_glUseProgram;
  }
  if (!strcmp(name, "glViewport")) {
    g_r_viewport = real;
    return (uintptr_t)f14_glViewport;
  }
  if (!strcmp(name, "glBlendFunc")) {
    g_r_blendfunc = real;
    return (uintptr_t)f14_glBlendFunc;
  }
  return 0;
}

/* ------------------------------------------------------------------- boot */
void fifa14_gl_boot(void) {
  if (g_booted)
    return;
  g_booted = 1;

  FIFA14_EGL_GetDisplay get_display = (FIFA14_EGL_GetDisplay)dcr_gl_lookup("eglGetDisplay");
  FIFA14_EGL_Initialize initialize = (FIFA14_EGL_Initialize)dcr_gl_lookup("eglInitialize");
  FIFA14_EGL_ChooseConfig choose_config = (FIFA14_EGL_ChooseConfig)dcr_gl_lookup("eglChooseConfig");
  FIFA14_EGL_CreateWindowSurface create_window =
      (FIFA14_EGL_CreateWindowSurface)dcr_gl_lookup("eglCreateWindowSurface");
  FIFA14_EGL_CreateContext create_context =
      (FIFA14_EGL_CreateContext)dcr_gl_lookup("eglCreateContext");
  FIFA14_EGL_MakeCurrent make_current = (FIFA14_EGL_MakeCurrent)dcr_gl_lookup("eglMakeCurrent");

  if (!get_display || !initialize || !choose_config || !create_window || !create_context ||
      !make_current) {
    debugPrintf("[gl] the EGL entry points are missing (glGetDisplay %p, initialize %p, "
                "chooseConfig %p, createWindowSurface %p, createContext %p, makeCurrent %p): "
                "no context, and the engine's glGetString is answered by the port's "
                "stand-in strings\n",
                (void *)get_display, (void *)initialize, (void *)choose_config,
                (void *)create_window, (void *)create_context, (void *)make_current);
    return;
  }

  /* Android's Java creates the surface; the EGL window is the runtime's own
   * (rt_window.c). The on-screen boot log has to let go of it first, exactly
   * as the rendering self-test does. */
  if (log_console_active())
    debugPrintf("[gl] the screen belongs to EGL from here on: the rest of the boot log is "
                "in debug.log only\n");
  log_console_close();
  dcr_window_prepare();

  int maj = 0, min = 0;
  g_display = get_display((void *)(uintptr_t)EGL_DEFAULT_DISPLAY);
  if (!g_display || !initialize(g_display, &maj, &min)) {
    debugPrintf("[gl] eglGetDisplay/eglInitialize failed: no context\n");
    g_display = NULL;
    return;
  }

  FIFA14_EGL_BindAPI bind_api = (FIFA14_EGL_BindAPI)dcr_gl_lookup("eglBindAPI");
  if (bind_api)
    bind_api(EGL_OPENGL_ES_API); /* EGL's default; what the self-test does */

  static const int cfg_attrs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                  EGL_RED_SIZE,      8,
                                  EGL_GREEN_SIZE,    8,
                                  EGL_BLUE_SIZE,     8,
                                  EGL_ALPHA_SIZE,    8,
                                  EGL_DEPTH_SIZE,    24,
                                  EGL_STENCIL_SIZE,  8,
                                  EGL_SURFACE_TYPE,  EGL_WINDOW_BIT,
                                  EGL_NONE};
  void *config = NULL;
  int nconfig = 0;
  if (!choose_config(g_display, cfg_attrs, &config, 1, &nconfig) || nconfig < 1) {
    /* A phone of FIFA 14's year is the plain 8888+24-bit case; if the GL cannot
     * do the stencil, the engine's depth/stencil work is the smaller loss. */
    static const int cfg_attrs_lite[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                         EGL_RED_SIZE,      8,
                                         EGL_GREEN_SIZE,    8,
                                         EGL_BLUE_SIZE,     8,
                                         EGL_DEPTH_SIZE,    16,
                                         EGL_NONE};
    nconfig = 0;
    if (!choose_config(g_display, cfg_attrs_lite, &config, 1, &nconfig) || nconfig < 1) {
      debugPrintf("[gl] eglChooseConfig found no config: no context\n");
      return;
    }
  }

  g_surface = create_window(g_display, config, (void *)nwindowGetDefault(), NULL);
  static const int ctx_attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  g_context = create_context(g_display, config, (void *)EGL_NO_CONTEXT, ctx_attrs);
  if (!g_surface || !g_context || !make_current(g_display, g_surface, g_surface, g_context)) {
    debugPrintf("[gl] surface %p / context %p / eglMakeCurrent failed: no context. The "
                "engine's glGetString is answered by the port's stand-in strings\n",
                g_surface, g_context);
    g_context = NULL;
    return;
  }
  g_current = 1;

  {
    FIFA14_EGL_QueryString egl_query = (FIFA14_EGL_QueryString)dcr_gl_lookup("eglQueryString");
    /* Through dcr_gl_lookup, so the port's own non-NULL wrapper answers. */
    FIFA14_GL_GetString gl_string = (FIFA14_GL_GetString)dcr_gl_lookup("glGetString");
    int w = 0, h = 0;
    dcr_window_size(&w, &h);
    debugPrintf("[gl] EGL context current (%dx%d, EGL %s \"%s\"): GL vendor \"%s\" | renderer "
                "\"%s\" | version \"%s\"\n",
                w, h, egl_query ? egl_query(g_display, EGL_VERSION) : "?",
                egl_query ? egl_query(g_display, EGL_VENDOR) : "?",
                gl_string ? gl_string(GL_VENDOR) : "?",
                gl_string ? gl_string(GL_RENDERER) : "?",
                gl_string ? gl_string(GL_VERSION) : "?");
  }
}

const char *fifa14_gl_renderer(void) {
  FIFA14_GL_GetString gl_string = (FIFA14_GL_GetString)dcr_gl_lookup("glGetString");
  return gl_string ? gl_string(GL_RENDERER) : "";
}

int fifa14_gl_current(void) { return g_current; }

/* -------------------------------------------------------------- the swap
 * Android's GLSurfaceView render thread: onDrawFrame() into the back buffer,
 * then its EGLHelper.swap() -> EGL.eglSwapBuffers(display, surface). The
 * engine does the first half only -- libFIFA14.so imports eglSwapBuffers but
 * never calls it (measured on hardware: ~4500 frames with "engine swaps: 0"
 * and not one "[egl] eglSwapBuffers" line) -- so the port does the second half
 * here, once per engine frame, from the frame loop.
 *
 * THE HOOK IS NOT A NO-OP. The engine's own eglSwapBuffers import, when it
 * ever calls it, resolves to the runtime's b_eglSwapBuffers (imports.c ->
 * gl_mesa.c), which calls the REAL eglSwapBuffers and counts the frame; that
 * hook was already correct. What was missing was the CALL: nothing invoked it.
 * Going through dcr_gl_lookup("eglSwapBuffers") here means the port's swap
 * reaches that same real swap, so the present hook, the capture, the frame
 * counter (dcr_gl_frames: the [loop] "engine swaps" and the watchdog's) and a
 * port's own hooks all see it exactly as if the engine had presented.
 *
 * RT_GL_SWAP_ENDS_FRAME is 0 for this port (port_config.h): the loop brackets
 * its own frames (dcr_boost_frame_begin/end), and b_eglSwapBuffers honours
 * that and does not re-bracket -- so calling it here does not change the
 * engine's pacing. */
int fifa14_gl_swap(void) {
  if (!g_current || !g_display || !g_surface)
    return 0; /* no context: nothing to present (logged by fifa14_gl_boot) */

  static FIFA14_EGL_SwapBuffers swap;
  static FIFA14_EGL_GetError get_error;
  static uint32_t nswap;
  if (!swap) {
    swap = (FIFA14_EGL_SwapBuffers)dcr_gl_lookup("eglSwapBuffers");
    get_error = (FIFA14_EGL_GetError)dcr_gl_lookup("eglGetError");
  }
  if (!swap)
    return 0;

  const unsigned r = swap(g_display, g_surface);
  const unsigned err = r ? 0u : (get_error ? get_error() : 0u);
  nswap++;
  /* the first few, then one every 300 frames (the [loop] window), and any
   * failure: enough to see the counter climb without flooding the card. */
  if (nswap <= 3 || nswap % 300u == 0u || !r)
    debugPrintf("[gl] swap #%u: eglSwapBuffers(%p, %p) -> %u err 0x%x%s\n", nswap, g_display,
                g_surface, r, err, r ? "" : "  <- the present FAILED");
  return r != 0;
}
