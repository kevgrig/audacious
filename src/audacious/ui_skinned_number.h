/*
 * Audacious - a cross-platform multimedia player
 * Copyright (c) 2007  Audacious development team.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef UISKINNEDNUMBER_H
#define UISKINNEDNUMBER_H

#include <gdk/gdk.h>
#include <gtk/gtkadjustment.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_SKINNED_NUMBER(obj)          GTK_CHECK_CAST (obj, ui_skinned_number_get_type (), UiSkinnedNumber)
#define UI_SKINNED_NUMBER_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, ui_skinned_number_get_type (), UiSkinnedNumberClass)
#define UI_SKINNED_IS_NUMBER(obj)       GTK_CHECK_TYPE (obj, ui_skinned_number_get_type ())

typedef struct _UiSkinnedNumber        UiSkinnedNumber;
typedef struct _UiSkinnedNumberClass   UiSkinnedNumberClass;

struct _UiSkinnedNumber {
    GtkWidget        widget;

    gint             x, y, width, height;
    gint             num;
    gboolean         double_size;
    GdkPixmap        *img;
    SkinPixmapId     skin_index;
    GtkWidget        *fixed;
};

struct _UiSkinnedNumberClass {
    GtkWidgetClass          parent_class;
    void (* doubled)        (UiSkinnedNumber *textbox);
};

GtkWidget* ui_skinned_number_new (GtkWidget *fixed, gint x, gint y, SkinPixmapId si);
GtkType ui_skinned_number_get_type(void);
void ui_skinned_number_set_number(GtkWidget *widget, gint num);

#ifdef __cplusplus
}
#endif

#endif
