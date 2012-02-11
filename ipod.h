/**
 * ipod.h
 * Modified: 02/02/2012
 */
#ifndef __DB_IPOD_H__
#define __DB_IPOD_H__

#include <gpod/itdb.h>
#include <stdio.h>
#include <deadbeef/deadbeef.h>
// Hacky: include artwork.h from the deadbeef repo so we can use the
// artwork plugin here.
#include "artwork.h"

extern DB_functions_t * deadbeef;
static DB_artwork_plugin_t * art_plugin = NULL;
static Itdb_iTunesDB * ipod_db = NULL;

/**
 * Function to get the selected tracks in the current playlist for copying
 * to the ipod.
 *
 * @author Chase Geigle
 * @return A glib defined GArray of the selected tracks in the current playlist
 */
GArray * ipod_get_selected_tracks();

/**
 * Wrapper function that gets string metadata for a given deadbeef playlist
 * item. 
 *
 * @param track Pointer to a Deadbeef playlist item to find metadata on
 * @param key Key that we're looking for (ex: "artist")
 * @return String metadata for that key, or "?" if it cannot be found.
 */
const char * ipod_get_db_meta(DB_playItem_t * track, const char * key);

/**
 * Wrapper function that gets integer metadata for a given deadbeef
 * playlist item.
 *
 * @param track Pointer to a Deadbeef playlist item to find metadata on
 * @param key Key that we're looking for (ex: "track")
 * @return Integer metdata for that key, or 0 if it cannot be found.
 */
int ipod_get_db_meta_int(DB_playItem_t * track, const char * key);

/**
 * Helper function that grabs the album artist for a track.
 *
 * @see playlist.c:2827
 * @param track Pointer to a Deadbeef playlist item to find the album artist for.
 * @return String containing the album artist.
 */
const char * ipod_get_db_albumartist(DB_playItem_t * track);

/**
 * Function to build an Itdb_Track from a Deadbeef playlist item. This is
 * what we use to convert our representations into the format that libgpod
 * understands. Note that this also handles artwork.
 *
 * @param track Pointer to a Deadbeef playlist item to convert.
 * @return A pointer to an Itdb_Track based on the playlist item.
 */
Itdb_Track * ipod_make_itdb_track(DB_playItem_t * track);

/**
 * Copies a given Deadbeef playlist item to the ipod.
 *
 * @param track Pointer to the Deadbeef playlist item to be copied.
 * @return Boolean value whether the copy was successful or not.
 */
gboolean ipod_copy_track(DB_playItem_t * track);

/**
 * Copies currently selected Deadbeef playlist items to the ipod. This is
 * currently the callback that happens when you click the track context
 * item menu for the plugin.
 *
 * @return Integer success code (C style)
 */
static int ipod_copy_tracks();

/**
 * Loads in the ipod database at our current mountpoint.
 */
void ipod_load_ipod_db();

/**
 * Frees our currently loaded ipod database.
 */
void ipod_free_ipod_db();

/**
 * Entry point for the plugin. Deadbeef will call this to initialize our
 * plugin.
 *
 * @return Integer success code (C style)
 */
int ipod_start();

/**
 * Cleanup entry point for the plugin. Deadbeef will call this before
 * exiting or otherwise unloading our plugin.
 *
 * @return Integer success code (C style)
 */
int ipod_stop();

/**
 * Entry point for Deadbeef. Sets up our deadbeef pointer.
 *
 * @param api Passed to us by Deadbeef, allows us to use its api inside of
 * the plugin.
 * @return Our plugin's interface.
 */
DB_plugin_t * ipod_load(DB_functions_t * api);
#endif
