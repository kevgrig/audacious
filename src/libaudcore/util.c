/*
 * util.c
 * Copyright 2009-2013 John Lindgren and Michał Lipski
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include "internal.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <glib.h>

#include "audstrings.h"
#include "runtime.h"

bool_t dir_foreach (const char * path, DirForeachFunc func, void * user)
{
    GDir * dir = g_dir_open (path, 0, NULL);
    if (! dir)
        return FALSE;

    const char * name;
    while ((name = g_dir_read_name (dir)))
    {
        char * full = filename_build (path, name);
        bool_t stop = func (full, name, user);
        str_unref (full);

        if (stop)
            break;
    }

    g_dir_close (dir);
    return TRUE;
}

char * write_temp_file (void * data, int64_t len)
{
    char * temp = filename_build (g_get_tmp_dir (), "audacious-temp-XXXXXX");
    SCOPY (name, temp);
    str_unref (temp);

    int handle = g_mkstemp (name);
    if (handle < 0)
    {
        fprintf (stderr, "Error creating temporary file: %s\n", strerror (errno));
        return NULL;
    }

    while (len)
    {
        int64_t written = write (handle, data, len);
        if (written < 0)
        {
            fprintf (stderr, "Error writing %s: %s\n", name, strerror (errno));
            close (handle);
            return NULL;
        }

        data = (char *) data + written;
        len -= written;
    }

    if (close (handle) < 0)
    {
        fprintf (stderr, "Error closing %s: %s\n", name, strerror (errno));
        return NULL;
    }

    return str_get (name);
}

/* Strips various common top-level folders from a filename.  The string passed
 * will not be modified, but the string returned will share the same memory.
 * Examples:
 *     "/home/john/folder/file.mp3" -> "folder/file.mp3"
 *     "/folder/file.mp3"           -> "folder/file.mp3" */

static char * skip_top_folders (char * name)
{
    static const char * home;
    static int len;

    if (! home)
    {
        home = g_get_home_dir ();
        len = strlen (home);

        if (len > 0 && home[len - 1] == G_DIR_SEPARATOR)
            len --;
    }

#ifdef _WIN32
    if (! g_ascii_strncasecmp (name, home, len) && name[len] == '\\')
#else
    if (! strncmp (name, home, len) && name[len] == '/')
#endif
        return name + len + 1;

#ifdef _WIN32
    if (g_ascii_isalpha (name[0]) && name[1] == ':' && name[2] == '\\')
        return name + 3;
#else
    if (name[0] == '/')
        return name + 1;
#endif

    return name;
}

/* Divides a filename into the base name, the lowest folder, and the
 * second lowest folder.  The string passed will be modified, and the strings
 * returned will use the same memory.  May return NULL for <first> and <second>.
 * Examples:
 *     "a/b/c/d/e.mp3" -> "e", "d",  "c"
 *     "d/e.mp3"       -> "e", "d",  NULL
 *     "e.mp3"         -> "e", NULL, NULL */

static void split_filename (char * name, char * * base, char * * first, char * * second)
{
    * first = * second = NULL;

    char * c;

    if ((c = strrchr (name, G_DIR_SEPARATOR)))
    {
        * base = c + 1;
        * c = 0;
    }
    else
    {
        * base = name;
        goto DONE;
    }

    if ((c = strrchr (name, G_DIR_SEPARATOR)))
    {
        * first = c + 1;
        * c = 0;
    }
    else
    {
        * first = name;
        goto DONE;
    }

    if ((c = strrchr (name, G_DIR_SEPARATOR)))
        * second = c + 1;
    else
        * second = name;

DONE:
    if ((c = strrchr (* base, '.')))
        * c = 0;
}

/* Separates the domain name from an internet URI.  The string passed will be
 * modified, and the string returned will share the same memory.  May return
 * NULL.  Examples:
 *     "http://some.domain.org/folder/file.mp3" -> "some.domain.org"
 *     "http://some.stream.fm:8000"             -> "some.stream.fm" */

static char * stream_name (char * name)
{
    if (! strncmp (name, "http://", 7))
        name += 7;
    else if (! strncmp (name, "https://", 8))
        name += 8;
    else if (! strncmp (name, "mms://", 6))
        name += 6;
    else
        return NULL;

    char * c;

    if ((c = strchr (name, '/')))
        * c = 0;
    if ((c = strchr (name, ':')))
        * c = 0;
    if ((c = strchr (name, '?')))
        * c = 0;

    return name;
}

static char * get_nonblank_field (const Tuple * tuple, int field)
{
    char * str = tuple ? tuple_get_str (tuple, field) : NULL;

    if (str && ! str[0])
    {
        str_unref (str);
        str = NULL;
    }

    return str;
}

static char * str_get_decoded (char * str)
{
    if (! str)
        return NULL;

    str_decode_percent (str, -1, str);
    return str_get (str);
}

/* Derives best guesses of title, artist, and album from a file name (URI) and
 * tuple (which may be NULL).  The returned strings are stringpooled or NULL. */

void describe_song (const char * name, const Tuple * tuple, char * * _title,
 char * * _artist, char * * _album)
{
    /* Common folder names to skip */
    static const char * const skip[] = {"music"};

    char * title = get_nonblank_field (tuple, FIELD_TITLE);
    char * artist = get_nonblank_field (tuple, FIELD_ARTIST);
    char * album = get_nonblank_field (tuple, FIELD_ALBUM);

    if (title && artist && album)
    {
DONE:
        * _title = title;
        * _artist = artist;
        * _album = album;
        return;
    }

    if (! strncmp (name, "file:///", 8))
    {
        char * filename = uri_to_display (name);
        if (! filename)
            goto DONE;

        SCOPY (buf, filename);

        char * base, * first, * second;
        split_filename (skip_top_folders (buf), & base, & first, & second);

        if (! title)
            title = str_get (base);

        for (int i = 0; i < ARRAY_LEN (skip); i ++)
        {
            if (first && ! g_ascii_strcasecmp (first, skip[i]))
                first = NULL;
            if (second && ! g_ascii_strcasecmp (second, skip[i]))
                second = NULL;
        }

        if (first)
        {
            if (second && ! artist && ! album)
            {
                artist = str_get (second);
                album = str_get (first);
            }
            else if (! artist)
                artist = str_get (first);
            else if (! album)
                album = str_get (first);
        }

        str_unref (filename);
    }
    else
    {
        SCOPY (buf, name);

        if (! title)
        {
            title = str_get_decoded (stream_name (buf));

            if (! title)
                title = str_get_decoded (buf);
        }
        else if (! artist)
            artist = str_get_decoded (stream_name (buf));
        else if (! album)
            album = str_get_decoded (stream_name (buf));
    }

    goto DONE;
}

char * last_path_element (char * path)
{
    char * slash = strrchr (path, G_DIR_SEPARATOR);
    return (slash && slash[1]) ? slash + 1 : NULL;
}
