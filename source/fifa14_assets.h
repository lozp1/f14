/* fifa14_assets.h -- the APK's assets/ where the game looks for them.
 *
 * Two halves, one subject:
 *   fifa14_blast_stubs.c  builds the index ONCE at boot (the zip's central
 *     directory: name -> local header / sizes / compression) and answers the
 *     three questions below; the JNI AssetManager (the engine's Java-side
 *     reads) is that file too.
 *   fifa14_assets.c       the engine's OWN file layer (EA::IO::FileStream /
 *     AssetStream opening "<game folder>/assets/<name>"), hooked through
 *     port_path_fixup(): the entry is taken from the APK, inflated when it is
 *     deflated, and written where that open will find it.
 * MIT. */
#ifndef FIFA14_ASSETS_H
#define FIFA14_ASSETS_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------- fifa14_blast_stubs.c */
/* The index's entry count (0 before fifa14_assets_init() ran). */
int fifa14_assets_count(void);
/* The asset `name`: 1 and its sizes when the index holds it; 0 when it does not
 * (or the index is not built). The name is normalized by the ONE rule the
 * JNI side's open()/openFd()/list() all use (fifa14_blast_stubs.c), so every
 * spelling the engine's paths arrive in is one entry: "EAMCore.ini",
 * "assets/EAMCore.ini", "/assets/EAMCore.ini", "assets:/EAMCore.ini",
 * "appbundle:/EAMCore.ini", "./assets/EAMCore.ini", "file:///.../assets/...".
 * `stored` is 1 for a STORE (method 0) entry, whose bytes are in the APK as
 * they are (0 = DEFLATE: its bytes have to be inflated, which
 * fifa14_asset_bytes() does). */
int fifa14_asset_info(const char *name, uint32_t *raw_size, uint32_t *comp_size, int *stored);
/* Its bytes: the entry INFLATED when it is deflated, as they are when stored.
 * malloc'd (the caller frees it), `size` gets their count. NULL when the entry
 * cannot be read or does not inflate to its recorded length. */
uint8_t *fifa14_asset_bytes(const char *name, uint32_t *size);
/* Called by fifa14_assets_init() once the walk is done: only from then on may
 * the port's path layer serve an asset (before it, every request is logged as
 * "the APK's assets/ is not indexed yet" and left to fail as it did before). */
void fifa14_asset_index_ready(void);

/* ---------------------------------------------------- fifa14_assets.c */
/* Called by fifa14_assets_init() BEFORE the walk: the port's own lock (the
 * extraction and the log both take it). */
void fifa14_asset_log_init(void);
/* THE CONTENT MANAGER'S SPEECH CONFIG (fifa14_assets.c has the whole measured
 * chain: FlowInitialization::GetRecommendedSyncAction @0x00dd3a94 returns 0 =
 * "no sync" when updateCheckPeriodDays is negative, and the APK ships 0, which
 * needs EA's server -- http://127.0.0.1 in this build -- for the description
 * check that then fails). This edits the bytes the port serves the engine's
 * AssetManager, IN PLACE and keeping their length (the engine measured them
 * with openFd().getLength()). 1 when something changed. Called for
 * "contentmanagerSpeech.cmconfig" only. */
int fifa14_cm_config_patch(uint8_t *bytes, uint32_t size);
/* What every open of a path inside assets/ did, and the distinct paths asked
 * for, one line each (port_run() calls it; also the port_setup log ends with
 * the index's line). */
void fifa14_assets_report(void);

/* ------------------------------------------------ the index's own checks */
/* Called at the end of fifa14_assets_init(), once the walk is done: it asks
 * the INDEX the same questions the engine's AssetManager will ask (open,
 * openFd, list) for the entries the engine reads first, in every spelling the
 * engine uses ("EAMCore.ini", "assets/EAMCore.ini", "appbundle:/EAMCore.ini",
 * ...), reads one of them back from the APK to prove the inflate path works,
 * and prints the top-level listing list("") returns. One run's log then says
 * by itself whether the asset layer can find what the engine asks for --
 * before the engine asks. `path` is only for the log. */
void fifa14_assets_selfcheck(const char *path);

/* --------------------------------- the last two EA::Jni::Contexts (stubs)
 * Each of EA::InAppWebBrowser::BrowserAndroid and
 * EA::VideoPlayer::PlayerAndroid owns a static EA::Jni::Context whose
 * m_javaVM only its own Java_com_ea_*_StartupNativeImpl fills; the engine
 * builds each class's EA::Jni::Delegate from that Context
 * (Delegate::Delegate(char const*, Context*) @0xe3a458) and calls
 * Context::GetEnv() on it (@0xe3a374), which faults while the field is 0
 * (config.h has the crash and the evidence). This drives both natives the way
 * the APK's Java would (fifa14_jni_class_start is idempotent: each native is
 * called once per process). Returns the number called now (0 = both were
 * already done, -1 = neither native is bound). `why` is only for the log. */
int fifa14_jni_class_start(const char *why);

#endif /* FIFA14_ASSETS_H */
