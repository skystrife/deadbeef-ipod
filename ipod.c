#include <gpod/itdb.h>
#include <stdio.h>
#include <deadbeef/deadbeef.h>

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

const char * ipod_get_db_meta(DB_playItem_t * track, const char * key)
{
    const char * meta;
    meta = deadbeef->pl_find_meta(track, key);
    if (!meta)
        meta = "?";
    return meta;
}

int ipod_get_db_meta_int(DB_playItem_t * track, const char * key)
{
    return deadbeef->pl_find_meta_int(track, key, 0);
}

// Taken from playlist.c:2827
const char * ipod_get_db_albumartist(DB_playItem_t * track)
{
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

Itdb_Track * ipod_make_itdb_track(DB_playItem_t * track)
{
    Itdb_Track * ipod_track;

    // construct
    ipod_track = itdb_track_new();

    // grab metadata
    ipod_track->title = g_strdup(ipod_get_db_meta(track, "title"));
    ipod_track->artist = g_strdup(ipod_get_db_meta(track, "artist"));
    ipod_track->album = g_strdup(ipod_get_db_meta(track, "album"));
    ipod_track->albumartist = g_strdup(ipod_get_db_albumartist(track));
    ipod_track->track_nr = ipod_get_db_meta_int(track, "track");
    ipod_track->cd_nr = ipod_get_db_meta_int(track, "disc");
    ipod_track->comment = g_strdup(ipod_get_db_meta(track, "comment"));
    ipod_track->tracklen = (int)(deadbeef->pl_get_item_duration(track) * 1000);
    g_print("Track length: %d\n", ipod_track->tracklen);

    return ipod_track;
}

gboolean ipod_copy_track(DB_playItem_t * track) {
    const char * filename;
    Itdb_Track * ipod_track;
    GError * error;
    gboolean copy_success;

    error = NULL;
    filename = deadbeef->pl_find_meta(track, ":URI");
    ipod_track = ipod_make_itdb_track(track);
    itdb_track_add(ipod_db, ipod_track, -1);
    itdb_playlist_add_track(itdb_playlist_mpl(ipod_db), ipod_track, -1);
    copy_success = itdb_cp_track_to_ipod(ipod_track, filename, &error);
    if (!copy_success) {
        itdb_track_free(ipod_track);
        if (error && error->message) {
            g_print("Error copying file to ipod, aborting with message:\n\t%s\n", error->message);
            g_error_free(error);
        }
    } else
        g_print("Successfully copied track to ipod!\n");
    return copy_success;
}

static int ipod_copy_tracks() {
    GArray * tracks;
    GError * error;
    gboolean copy_success;
    gint i;

    tracks = ipod_get_selected_tracks();
    if (tracks) {
        for (i = 0; i < tracks->len; i++) {
            copy_success = ipod_copy_track(g_array_index(tracks, DB_playItem_t *, i));
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
    return 0;
}

void ipod_load_ipod_db() {
    // TODO: load in our ipod mountpoint from a file
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

int ipod_start()
{
    ipod_load_ipod_db();
    return 0;
}

int ipod_stop()
{
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

static DB_plugin_action_t * ipod_get_actions(DB_playItem_t * it)
{
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
    .plugin.website = "",
    .plugin.start = ipod_start,
    .plugin.stop = ipod_stop,
    .plugin.get_actions = ipod_get_actions
};

DB_plugin_t * ipod_load (DB_functions_t * api)
{
    deadbeef = api;
    return DB_PLUGIN(&plugin);
}
