/*
 * WinDoesNot: a drive of "This PC" (icon, name, fill bar, free space).
 * The percentage and the free-space text come from the Files extension windoesnot-computer.py
 * ("wdn_percent", "wdn_space"); without them the cell shows just the icon and the name.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-computer-cell.h"

#include <glib/gi18n.h>

#include "nautilus-file.h"
#include "nautilus-list-base.h"
#include "nautilus-view-item.h"

struct _NautilusComputerCell
{
    NautilusViewCell parent_instance;

    GSignalGroup *item_signal_group;
    gulong collapsed_handler;

    GtkWidget *box;
    GtkWidget *icon;
    GtkWidget *name;
    GtkWidget *bar;
    GtkWidget *space;
    GtkWidget *eject_button;
};

G_DEFINE_FINAL_TYPE (NautilusComputerCell, nautilus_computer_cell, NAUTILUS_TYPE_VIEW_CELL)

static gint
cell_section (NautilusFile *file)
{
    static GQuark order_q = 0;
    g_autofree char *order = NULL;

    if (order_q == 0)
    {
        order_q = g_quark_from_static_string ("wdn_order");
    }
    order = nautilus_file_get_string_attribute_q (file, order_q);

    return (order != NULL && order[0] == '4') ? 1 : 0;
}

static void
update_collapsed (NautilusComputerCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    NautilusListBase *view = nautilus_view_cell_get_view (NAUTILUS_VIEW_CELL (self));
    GtkWidget *row = gtk_widget_get_parent (GTK_WIDGET (self));
    guint mask = 0;
    gboolean collapsed;

    if (item == NULL || view == NULL)
    {
        return;
    }

    g_object_get (view, "collapsed-mask", &mask, NULL);
    collapsed = (mask & (1u << cell_section (nautilus_view_item_get_file (item)))) != 0;

    gtk_widget_set_visible (self->box, !collapsed);
    if (row != NULL)
    {
        if (collapsed)
        {
            gtk_widget_add_css_class (row, "wdn-collapsed");
        }
        else
        {
            gtk_widget_remove_css_class (row, "wdn-collapsed");
        }
    }
}

static void
update_icon_size (NautilusComputerCell *self)
{
    guint size = 48;

    g_object_get (self, "icon-size", &size, NULL);
    gtk_image_set_pixel_size (GTK_IMAGE (self->icon), CLAMP ((gint) size, 16, 128));
}

static void
update_from_file (NautilusComputerCell *self)
{
    g_autoptr (NautilusViewItem) item = nautilus_view_cell_get_item (NAUTILUS_VIEW_CELL (self));
    static GQuark percent_q = 0, space_q = 0;
    NautilusFile *file;
    g_autoptr (GIcon) icon = NULL;
    g_autofree char *percent_text = NULL;
    g_autofree char *space_text = NULL;
    const char *name = NULL;

    g_return_if_fail (item != NULL);

    if (percent_q == 0)
    {
        percent_q = g_quark_from_static_string ("wdn_percent");
        space_q = g_quark_from_static_string ("wdn_space");
    }

    file = nautilus_view_item_get_file (item);
    icon = nautilus_file_get_gicon (file, NAUTILUS_FILE_ICON_FLAGS_NONE);
    gtk_image_set_from_gicon (GTK_IMAGE (self->icon), icon);

    name = nautilus_file_get_display_name (file);
    gtk_label_set_label (GTK_LABEL (self->name), name);

    percent_text = nautilus_file_get_string_attribute_q (file, percent_q);
    space_text = nautilus_file_get_string_attribute_q (file, space_q);

    if (percent_text != NULL && percent_text[0] != '\0')
    {
        double percent = g_ascii_strtod (percent_text, NULL);

        gtk_level_bar_set_value (GTK_LEVEL_BAR (self->bar), CLAMP (percent, 0, 100));
        gtk_widget_set_visible (self->bar, TRUE);
        if (percent >= 90)
        {
            gtk_widget_add_css_class (self->bar, "wdn-full");
        }
        else
        {
            gtk_widget_remove_css_class (self->bar, "wdn-full");
        }
    }
    else
    {
        gtk_widget_set_visible (self->bar, FALSE);
    }

    gtk_label_set_label (GTK_LABEL (self->space), space_text != NULL ? space_text : "");
    gtk_widget_set_visible (self->space, space_text != NULL && space_text[0] != '\0');
    gtk_widget_set_visible (self->eject_button, nautilus_file_can_unmount (file));

    update_icon_size (self);
    update_collapsed (self);
}

static void
on_eject_clicked (NautilusComputerCell *self)
{
    /* Select the item first, because view.unmount-volume acts on the selection. */
    gtk_widget_activate_action (GTK_WIDGET (self), "listitem.select", "(bb)", FALSE, FALSE);
    gtk_widget_activate_action (GTK_WIDGET (self), "view.unmount-volume", NULL);
}

static void
on_view_changed (NautilusComputerCell *self)
{
    NautilusListBase *view = nautilus_view_cell_get_view (NAUTILUS_VIEW_CELL (self));

    if (view != NULL && self->collapsed_handler == 0)
    {
        self->collapsed_handler = g_signal_connect_swapped (view, "notify::collapsed-mask",
                                                            G_CALLBACK (update_collapsed), self);
    }
}

static void
nautilus_computer_cell_init (NautilusComputerCell *self)
{
    GtkWidget *texts = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

    self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start (self->box, 12);
    gtk_widget_set_margin_end (self->box, 12);
    gtk_widget_set_margin_top (self->box, 8);
    gtk_widget_set_margin_bottom (self->box, 8);

    self->icon = gtk_image_new ();
    gtk_image_set_pixel_size (GTK_IMAGE (self->icon), 48);
    gtk_widget_set_valign (self->icon, GTK_ALIGN_CENTER);

    self->name = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (self->name), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (self->name), PANGO_ELLIPSIZE_END);

    self->bar = gtk_level_bar_new_for_interval (0, 100);
    gtk_widget_add_css_class (self->bar, "wdn-drive-bar");
    gtk_widget_set_size_request (self->bar, 280, -1);
    gtk_widget_set_halign (self->bar, GTK_ALIGN_START);

    self->space = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (self->space), 0.0);
    gtk_widget_add_css_class (self->space, "dim-label");
    gtk_widget_add_css_class (self->space, "caption");

    gtk_box_append (GTK_BOX (texts), self->name);
    gtk_box_append (GTK_BOX (texts), self->bar);
    gtk_box_append (GTK_BOX (texts), self->space);
    gtk_widget_set_valign (texts, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (texts, TRUE);

    self->eject_button = gtk_button_new_from_icon_name ("media-eject-symbolic");
    gtk_widget_add_css_class (self->eject_button, "flat");
    gtk_widget_set_valign (self->eject_button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (self->eject_button, _("Unmount"));
    g_signal_connect_swapped (self->eject_button, "clicked", G_CALLBACK (on_eject_clicked), self);
    g_signal_connect_swapped (self, "notify::icon-size", G_CALLBACK (update_icon_size), self);

    gtk_box_append (GTK_BOX (self->box), self->icon);
    gtk_box_append (GTK_BOX (self->box), texts);
    gtk_box_append (GTK_BOX (self->box), self->eject_button);
    gtk_widget_set_parent (self->box, GTK_WIDGET (self));

    /* Connect automatically to an item. */
    self->item_signal_group = g_signal_group_new (NAUTILUS_TYPE_VIEW_ITEM);
    g_signal_group_connect_swapped (self->item_signal_group, "notify::loading",
                                    (GCallback) update_from_file, self);
    g_signal_group_connect_swapped (self->item_signal_group, "file-changed",
                                    (GCallback) update_from_file, self);
    g_signal_connect_object (self->item_signal_group, "bind",
                             (GCallback) update_from_file, self,
                             G_CONNECT_SWAPPED);

    g_object_bind_property (self, "item",
                            self->item_signal_group, "target",
                            G_BINDING_SYNC_CREATE);
}

static void
nautilus_computer_cell_dispose (GObject *object)
{
    NautilusComputerCell *self = (NautilusComputerCell *) object;

    g_clear_pointer (&self->box, gtk_widget_unparent);

    G_OBJECT_CLASS (nautilus_computer_cell_parent_class)->dispose (object);
}

static void
nautilus_computer_cell_finalize (GObject *object)
{
    NautilusComputerCell *self = (NautilusComputerCell *) object;

    g_clear_object (&self->item_signal_group);
    G_OBJECT_CLASS (nautilus_computer_cell_parent_class)->finalize (object);
}

static void
nautilus_computer_cell_class_init (NautilusComputerCellClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = nautilus_computer_cell_dispose;
    object_class->finalize = nautilus_computer_cell_finalize;

    gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

NautilusViewCell *
nautilus_computer_cell_new (NautilusListBase *view)
{
    NautilusViewCell *cell = NAUTILUS_VIEW_CELL (g_object_new (NAUTILUS_TYPE_COMPUTER_CELL,
                                                               "view", view,
                                                               NULL));

    on_view_changed (NAUTILUS_COMPUTER_CELL (cell));

    return cell;
}
