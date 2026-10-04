/*
 *   moofileview/moofileview-tests.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The parts of the file view that can be asked a question without a filesystem
 * and without a window: pure functions over what stat() reported.
 */

#include "moofileview/moofileview-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "moofileview/moofile-private.h"
#include "moofileview/mooiconview.h"
#include "moofileview/moofileview-impl.h"
#include "moofileview/moobookmarkmgr.h"
#include "mooutils/moomarkup.h"


static void
test_file_info_for_stat (void)
{
    MgwStatBuf buf;

    /* stat() reports exactly one type, so each case is one bit on a buffer
       that is otherwise zero. */
    memset (&buf, 0, sizeof buf);
    buf.isdir = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, MOO_FILE_INFO_IS_DIR);

    memset (&buf, 0, sizeof buf);
    buf.isblk = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, MOO_FILE_INFO_IS_BLOCK_DEV);

    memset (&buf, 0, sizeof buf);
    buf.ischr = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, MOO_FILE_INFO_IS_CHAR_DEV);

    memset (&buf, 0, sizeof buf);
    buf.isfifo = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, MOO_FILE_INFO_IS_FIFO);

    memset (&buf, 0, sizeof buf);
    buf.issock = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, MOO_FILE_INFO_IS_SOCKET);

    /* A regular file is the one kind with no bit of its own: the file view
       draws it from its mime type and not from its type bit. */
    memset (&buf, 0, sizeof buf);
    buf.isreg = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, 0);

    /* A symbolic link is not a type here: _moo_file_stat() resolves it and
       asks this about the target. The link bit alone says nothing. */
    memset (&buf, 0, sizeof buf);
    buf.islnk = TRUE;
    g_assert_cmpint (_moo_file_info_for_stat (&buf), ==, 0);
}


static void
test_drag_scroll_delta (void)
{
    const int width = 200;
    /* A tenth of the view on either side, so 20 pixels; x == 20 is already
       past it and x == 179 is the last pixel that still scrolls. */
    const int margin = 20;

    /* The middle of the view does not scroll at all. */
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (width / 2, width), ==, 0);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (margin, width), ==, 0);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (width - 1 - margin, width), ==, 0);

    /* Left of the margin scrolls left, right of it scrolls right. */
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (0, width), <, 0);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (width - 1, width), >, 0);

    /* The two edges are mirror images of each other. */
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (0, width), ==,
                     -_moo_icon_view_drag_scroll_delta (width - 1, width));

    /* Faster the closer to the edge: the step just inside the margin is the
       slowest one, and the edge is within four times it. */
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (0, width), <,
                     _moo_icon_view_drag_scroll_delta (margin - 1, width));
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (margin - 1, width), <, 0);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (margin - 1, width), >, -20);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (0, width), >=, -60);

    /* A view too narrow to have a margin of its own still scrolls, and does
       not divide by a zero-width margin. */
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (0, 1), <, 0);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (0, 40), <, 0);
    g_assert_cmpint (_moo_icon_view_drag_scroll_delta (39, 40), >, 0);
}


static void
test_key_is_text_input (void)
{
    /* The keys typing is made of: letters, digits, punctuation, space, and
       the characters outside ASCII that a keyboard can produce directly. */
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_a));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_Z));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_0));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_period));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_underscore));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_space));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_asciitilde));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_Cyrillic_a));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_adiaeresis));

    /* Navigation keys belong to the list: typing must not steal the arrows,
       the page keys or the ends of the list. */
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Up));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Down));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Left));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Right));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Home));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_End));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Page_Up));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Page_Down));

    /* Neither are the keys that act on the list or leave it. */
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Return));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_KP_Enter));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Escape));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Tab));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_ISO_Left_Tab));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_BackSpace));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Delete));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Insert));

    /* Modifiers and function keys produce no text of their own. */
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Shift_L));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Control_R));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Alt_L));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Caps_Lock));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Num_Lock));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_F1));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_F12));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_Menu));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_VoidSymbol));

    /* The keypad is split: the digits and the operators type, the arrows on
       it do what the arrows do. */
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_KP_0));
    g_assert_true (_moo_file_view_key_is_text_input (GDK_KEY_KP_Add));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_KP_Up));
    g_assert_false (_moo_file_view_key_is_text_input (GDK_KEY_KP_Home));
}


static void
test_bookmark_icon_name_for_stock (void)
{
    /* The stock ids the old code wrote, as freedesktop names. */
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-directory"), ==, "folder");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-home"), ==, "go-home");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-harddisk"), ==, "drive-harddisk");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-network"), ==, "network-workgroup");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-cdrom"), ==, "media-optical");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-floppy"), ==, "media-floppy");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-file"), ==, "text-x-generic");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-open"), ==, "document-open");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("moo-folder"), ==, "folder");

    /* A stock id that nothing knows is a folder; an icon name is kept. */
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("gtk-no-such-id"), ==, "folder");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("user-desktop"), ==, "user-desktop");
    g_assert_cmpstr (_moo_bookmark_icon_name_for_stock ("go-home"), ==, "go-home");
}


static char *
bookmark_icon_at (MooBookmarkMgr *mgr,
                  int             n)
{
    GtkTreeModel *model = _moo_bookmark_mgr_get_model (mgr);
    GtkTreeIter iter;
    MooBookmark *bookmark = NULL;

    g_assert_true (gtk_tree_model_iter_nth_child (model, &iter, NULL, n));
    gtk_tree_model_get (model, &iter, MOO_BOOKMARK_MGR_COLUMN_BOOKMARK, &bookmark, -1);
    g_assert_nonnull (bookmark);
    char *icon = g_strdup (bookmark->icon_name);
    _moo_bookmark_free (bookmark);
    return icon;
}


/* An rc file as the code that stored stock ids wrote it. */
static void
test_bookmark_load_old_rc (void)
{
    static const char rc[] =
        "<medit-prefs><FileSelector><bookmarks>"
        "<bookmark label=\"Home\" icon=\"gtk-home\">/home/x</bookmark>"
        "<separator/>"
        "<bookmark label=\"Disk\" icon=\"gtk-harddisk\">/mnt</bookmark>"
        "<bookmark label=\"Odd\" icon=\"gtk-no-such-id\">/odd</bookmark>"
        "<bookmark label=\"New\" icon=\"user-desktop\">/desk</bookmark>"
        "<bookmark label=\"Bare\">/bare</bookmark>"
        "</bookmarks></FileSelector></medit-prefs>";

    MooMarkupDoc *doc = moo_markup_parse_memory (rc, -1, NULL);
    g_assert_nonnull (doc);
    MooMarkupNode *root = moo_markup_get_element (moo_markup_get_root_element (doc, "medit-prefs"), "FileSelector/bookmarks");
    g_assert_nonnull (root);

    MooBookmarkMgr *mgr = MOO_BOOKMARK_MGR (g_object_new (MOO_TYPE_BOOKMARK_MGR, (const char*) NULL));
    _moo_bookmark_mgr_load_node (mgr, root);

    g_autofree char *i0 = bookmark_icon_at (mgr, 0);
    g_autofree char *i2 = bookmark_icon_at (mgr, 2);
    g_autofree char *i3 = bookmark_icon_at (mgr, 3);
    g_autofree char *i4 = bookmark_icon_at (mgr, 4);
    g_assert_cmpstr (i0, ==, "go-home");
    g_assert_cmpstr (i2, ==, "drive-harddisk");
    g_assert_cmpstr (i3, ==, "folder");
    g_assert_cmpstr (i4, ==, "user-desktop");
    g_autofree char *i5 = bookmark_icon_at (mgr, 5);
    g_assert_null (i5);

    g_object_unref (mgr);
    moo_markup_doc_unref (doc);
}


void
_moo_add_moofileview_unit_tests (void)
{
    g_test_add_func ("/moofileview/file/info-for-stat", test_file_info_for_stat);
    g_test_add_func ("/moofileview/icon-view/drag-scroll-delta", test_drag_scroll_delta);
    g_test_add_func ("/moofileview/key-is-text-input", test_key_is_text_input);
    g_test_add_func ("/moofileview/bookmark/icon-name-for-stock", test_bookmark_icon_name_for_stock);
    g_test_add_func ("/moofileview/bookmark/load-old-rc", test_bookmark_load_old_rc);
}

#endif /* MOO_ENABLE_UNIT_TESTS */
