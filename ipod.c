/**
 * ipod.c
 * Modified: 02/11/2012
 */
#include "ipod.h"
DB_functions_t * deadbeef;

GArray * ipod_get_selected_tracks() {
    // mostly copied from the converter plugin
    GArray * tracks = NULL;
    int nsel;

    deadbeef->pl_lock();
    nsel = deadbeef->pl_getselcount();

    if (nsel > 0) {
        tracks = g_array_sized_new(FALSE, FALSE, sizeof(DB_playItem_t *), nsel);
        if (tracks) {
            DB_playItem_t * it = deadbeef->pl_get_first(PL_MAIN);
            while (it) {
                if (deadbeef->pl_is_selected(it)) {
                    deadbeef->pl_item_ref(it);
                    g_array_append_val(tracks, it);
                }
                DB_playItem_t * next = deadbeef->pl_get_next(it, PL_MAIN);
                deadbeef->pl_item_unref(it);
                it = next;
            }
        }
    }
    deadbeef->pl_unlock();
    return tracks;
}

const char * ipod_get_db_meta(DB_playItem_t * track, const char * key) {
    // special case album artist
    if (g_strcmp0("album artist", key) == 0)
        return ipod_get_db_albumartist(track);
    // otherwise get meta normally
    const char * meta;
    meta = deadbeef->pl_find_meta(track, key);
    if (!meta)
        meta = "?";
    return meta;
}

int ipod_get_db_meta_int(DB_playItem_t * track, const char * key) {
    return deadbeef->pl_find_meta_int(track, key, 0);
}

// Taken from playlist.c:2827
const char * ipod_get_db_albumartist(DB_playItem_t * track) {
    const char * meta;
    meta = deadbeef->pl_find_meta(track, "band");
    if (!meta) {
        meta = deadbeef->pl_find_meta(track, "album artist");
        if (!meta) {
            meta = deadbeef->pl_find_meta(track, "albumartist");
            if (!meta) {
                meta = deadbeef->pl_find_meta(track, "artist");
            }
        }
    }
    return meta;
}

Itdb_Track * ipod_make_itdb_track(DB_playItem_t * track) {
    Itdb_Track * ipod_track;

    // construct
    ipod_track = itdb_track_new();

    // grab metadata
    ipod_track->title = g_strdup(ipod_get_db_meta(track, "title"));
    ipod_track->artist = g_strdup(ipod_get_db_meta(track, "artist"));
    ipod_track->album = g_strdup(ipod_get_db_meta(track, "album"));
    ipod_track->albumartist = g_strdup(ipod_get_db_meta(track, "album artist"));
    ipod_track->track_nr = ipod_get_db_meta_int(track, "track");
    ipod_track->cd_nr = ipod_get_db_meta_int(track, "disc");
    ipod_track->comment = g_strdup(ipod_get_db_meta(track, "comment"));
    ipod_track->tracklen = (int)(deadbeef->pl_get_item_duration(track) * 1000);

    // grab artwork: first grab filename from deadbeef artwork plugin, then
    // assign it to the track with libgpod
    char * art_file = art_plugin->get_album_art_sync(
            ipod_get_db_meta(track, ":URI"),
            ipod_track->artist,
            ipod_track->album,
            -1
        );
    if (art_file != NULL) {
        itdb_track_set_thumbnails(ipod_track, art_file);
        g_free(art_file);
    }

    return ipod_track;
}

char * ipod_convert_for_ipod(DB_playItem_t * track) {
    // find our preset
    ddb_encoder_preset_t * preset = converter_plugin->encoder_preset_get_list();
    while (preset && g_strcmp0(preset->title, "iPod Convert") != 0)
        preset = preset->next;
    if (!preset) {
        g_print("Error: failed to find preset...\n");
        return NULL;
    }
    // get our output path
    char out_folder[] = "/tmp/deadbeef_ipod_convertXXXXXX";
    g_mkdtemp(out_folder);
    g_print(out_folder);
    char outpath[2000];
    converter_plugin->get_output_path(track, out_folder, "%a - %t", preset, outpath, sizeof(outpath));
    /*
     * Invoke converter plugin:
     *  - pass in track
     *  - make temporary directory to store our file
     *  - pass in a sane default for the filename
     *  - use encoder default bps
     *  - doesn't matter what we pass in for is float since it will be detected
     *  - preserve folder structure? doesn't appear to be used
     *  - root folder? doesn't appear to be used
     *  - don't use a dsp preset
     *  - store whether we were cancelled
     */
    int cancelled = 0;
    int success = converter_plugin->convert(track, out_folder, "%a - %t", -1, 0, 0, NULL, preset, NULL, &cancelled);
    if (cancelled || success == -1)
        return NULL;
    g_print("Finished converting for iPod...\n");
    g_print("Wrote to %s...\n", outpath);
    return g_strdup(outpath);
}

gboolean ipod_copy_track(DB_playItem_t * track) {
    char * filename;
    char * filetype;
    Itdb_Track * ipod_track;
    GError * error;
    gboolean copy_success;
    gboolean converted;

    if (track == NULL) {
        g_print("Error: Track is null!!");
        return FALSE;
    }

    // construct our itdb representation
    error = NULL;
    ipod_track = ipod_make_itdb_track(track);
    itdb_track_add(ipod_db, ipod_track, -1);
    itdb_playlist_add_track(itdb_playlist_mpl(ipod_db), ipod_track, -1);

    // determine if we need to convert
    filename = g_strdup(deadbeef->pl_find_meta(track, ":URI"));
    filetype = g_strdup(deadbeef->pl_find_meta(track, ":FILETYPE"));
    if (g_strcmp0("MP3", filetype) != 0 && g_strcmp0("MP4 AAC", filetype) != 0)
        filename = ipod_convert_for_ipod(track);

    if (filename == NULL) {
        copy_success = FALSE;
        g_print("Error copying file to ipod, aborting with message:\n\tNULL filename\n");
        goto ipod_copy_track_exit;
    }
    copy_success = itdb_cp_track_to_ipod(ipod_track, filename, &error);
    if (!copy_success) {
        itdb_track_free(ipod_track);
        if (error && error->message) {
            g_print("Error copying file to ipod, aborting with message:\n\t%s\n", error->message);
            g_error_free(error);
        }
    } else
        g_print("Successfully copied track to ipod!\n");
ipod_copy_track_exit:
    g_free(filename);
    g_free(filetype);
    return copy_success;
}

void ipod_copy_tracks_worker(void * ctx) {
    GArray * tracks;
    GError * error;
    gboolean copy_success;
    gint i;

    tracks = ipod_get_selected_tracks();
    if (tracks) {
        for (i = 0; i < tracks->len; i++) {
            DB_playItem_t * curr_track = g_array_index(tracks, DB_playItem_t *, i);
            copy_success = ipod_copy_track(curr_track);
            if (!copy_success)
                break;
        }
        for (i = 0; i < tracks->len; i++)
            deadbeef->pl_item_unref(g_array_index(tracks, DB_playItem_t *, i));
        g_array_free(tracks, FALSE);
    }
    error = NULL;
    if (ipod_db != NULL)
        itdb_write(ipod_db, &error);
    if (error && error->message) {
        g_print("Error writing ipod database:\n\t%s\n", error->message);
        g_error_free(error);
    } else
        g_print("Successfully wrote ipod database!\n");
}

static int ipod_copy_tracks() {
    intptr_t tid = deadbeef->thread_start(ipod_copy_tracks_worker, NULL);
    deadbeef->thread_detach(tid);
    return 0;
}

void ipod_load_ipod_db() {
    // TODO: load in our ipod mountpoint from a file
    g_print("Loading ipod db...\n");
    GError * error = NULL;
    if (ipod_db != NULL)
        itdb_free(ipod_db);
    ipod_db = itdb_parse("/media/IPOD", &error);
    if (error && error->message) {
        g_print("Error loading ipod database:\n\t%s", error->message);
        g_error_free(error);
    } else
        g_print("Successfully loaded ipod database!");
}

void ipod_free_ipod_db() {
    itdb_free(ipod_db);
    ipod_db = NULL;
}

int ipod_start() {
    g_print("Starting iPod plugin...\n");
    ipod_load_ipod_db();
    art_plugin = (DB_artwork_plugin_t *) deadbeef->plug_get_for_id("artwork");
    converter_plugin = (ddb_converter_t *) deadbeef->plug_get_for_id("converter");
    return 0;
}

int ipod_stop() {
    g_print("Stopping iPod plugin...\n");
    ipod_free_ipod_db();
    return 0;
}

static DB_plugin_action_t ipod_action = {
    .title = "Copy to iPod",
    .name = "ipod",
    .flags = DB_ACTION_CAN_MULTIPLE_TRACKS | DB_ACTION_ALLOW_MULTIPLE_TRACKS | DB_ACTION_SINGLE_TRACK,
    .callback = ipod_copy_tracks,
    .next = NULL
};

static DB_plugin_action_t * ipod_get_actions(DB_playItem_t * it) {
    return &ipod_action;
}

static DB_misc_t plugin = {
    .plugin.api_vmajor = 1,
    .plugin.api_vminor = 0,
    .plugin.version_major = 0,
    .plugin.version_minor = 1,
    .plugin.type = DB_PLUGIN_MISC,
    .plugin.name = "iPod Support",
    .plugin.id = "ipod",
    .plugin.descr = "Copies supported formats to an iPod device",
    .plugin.copyright = 
        "Copyright (C) 2012 Chase Geigle <sky@skystrife.com>\n"
        "\n"
        "This program is free software; you can redistribute it and/or\n"
         "modify it under the terms of the GNU General Public License\n"
         "as published by the Free Software Foundation; either version 2\n"
         "of the License, or (at your option) any later version.\n"
         "\n"
         "This program is distributed in the hope that it will be useful,\n"
         "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
         "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
         "GNU General Public License for more details.\n"
         "\n"
         "You should have received a copy of the GNU General Public License\n"
         "along with this program; if not, write to the Free Software\n"
         "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"
    ,
    .plugin.website = "https://github.com/skystrife/deadbeef-ipod",
    .plugin.start = ipod_start,
    .plugin.stop = ipod_stop,
    .plugin.get_actions = ipod_get_actions
};

DB_plugin_t * ipod_load (DB_functions_t * api) {
    deadbeef = api;
    return DB_PLUGIN(&plugin);
}
