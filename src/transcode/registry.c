/* Transcoder registry — the extension point (SPEC §6). v1 ships the built-in
 * USD/OKF/A2A/MAVLink/conv/robotdesc functions directly; this registry lets a
 * caller register additional codecs by name. */
#include "fluxmeme/codec.h"
#include <string.h>

#define FLUX_CODEC_SLOTS 32
static flux_codec_t g_codecs[FLUX_CODEC_SLOTS];
static int g_count = 0;

flux_status_t flux_codec_register(const flux_codec_t* codec) {
    if (!codec || !codec->name) return FLUX_ERR_ARG;
    if (g_count >= FLUX_CODEC_SLOTS) return FLUX_ERR_RANGE;
    for (int i = 0; i < g_count; ++i)
        if (strcmp(g_codecs[i].name, codec->name) == 0) {
            g_codecs[i] = *codec; /* replace */
            return FLUX_OK;
        }
    g_codecs[g_count++] = *codec;
    return FLUX_OK;
}

const flux_codec_t* flux_codec_find(const char* name) {
    if (!name) return NULL;
    for (int i = 0; i < g_count; ++i)
        if (strcmp(g_codecs[i].name, name) == 0) return &g_codecs[i];
    return NULL;
}
