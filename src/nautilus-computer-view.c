/*
 * Copyright (C) 2024 The GNOME project contributors; WinDoesNot: drives view "This PC"
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-computer-view.h"

#include <glib/gi18n.h>

#include "nautilus-file.h"
#include "nautilus-global-preferences.h"
#include "nautilus-list-base-private.h"
#include "nautilus-computer-cell.h"
#include "nautilus-scheme.h"
#include "nautilus-view-item.h"
#include "nautilus-view-model.h"

struct _NautilusComputerView
{
    NautilusListBase parent_instance;

    GtkListView *view_ui;
    guint collapsed_mask;   /* bit n set: section n is folded */
    gint zoom_level;
    GtkFilter *filter;      /* hides the drive item of the system disk */
    guint refilter_id;
    guint refilter_rounds;
};

enum
{
    PROP_0,
    PROP_COLLAPSED_MASK,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_FINAL_TYPE (NautilusComputerView, nautilus_computer_view, NAUTILUS_TYPE_LIST_BASE)

#define get_view_item(li) \
        (NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (gtk_list_item_get_item (li)))))

static const NautilusViewInfo computer_view_info =
{
    .view_id = NAUTILUS_VIEW_COMPUTER_ID,
    .zoom_level_min = NAUTILUS_LIST_ZOOM_LEVEL_SMALL,
    .zoom_level_max = NAUTILUS_LIST_ZOOM_LEVEL_LARGE,
    .zoom_level_standard = NAUTILUS_LIST_ZOOM_LEVEL_MEDIUM,
};

static NautilusViewInfo
real_get_view_info (NautilusListBase *list_base)
{
    return computer_view_info;
}

enum
{
    SECTION_DRIVES,     /* system disk and local disks */
    SECTION_EXTERNAL,   /* USB sticks, removable disks, optical drives, floppies */
    SECTION_NETWORK,
    N_SECTIONS,
};

/* The Files extension windoesnot-computer.py sets "wdn_order" ('0' system disk, '1' local disk, '2' removable disk,
 * '3' optical drive, '4' network) */
static inline gint
get_section (NautilusViewItem *item)
{
    static GQuark order_q = 0;
    NautilusFile *file = nautilus_view_item_get_file (item);
    g_autofree char *order = NULL;

    if (order_q == 0)
    {
        order_q = g_quark_from_static_string ("wdn_order");
    }

    order = nautilus_file_get_string_attribute_q (file, order_q);

    if (order == NULL)
    {
        return SECTION_DRIVES;
    }
    switch (order[0])
    {
        case '2':
        case '3':
        {
            return SECTION_EXTERNAL;
        }

        case '4':
        {
            return SECTION_NETWORK;
        }

        default:
        {
            return SECTION_DRIVES;
        }
    }
}

static gint
sort_computer_sections (gconstpointer a,
                        gconstpointer b,
                        gpointer      user_data)
{
    GtkTreeListRow *row_a = GTK_TREE_LIST_ROW ((gpointer) a);
    GtkTreeListRow *row_b = GTK_TREE_LIST_ROW ((gpointer) b);
    g_autoptr (NautilusViewItem) item_a = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row_a));
    g_autoptr (NautilusViewItem) item_b = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row_b));

    return get_section (item_a) - get_section (item_b);
}

static gint
sort_computer_items (gconstpointer a,
                     gconstpointer b,
                     gpointer      user_data)
{
    NautilusViewItem *item_a = NAUTILUS_VIEW_ITEM ((gpointer) a);
    NautilusViewItem *item_b = NAUTILUS_VIEW_ITEM ((gpointer) b);

    return nautilus_file_compare_for_sort (nautilus_view_item_get_file (item_a),
                                           nautilus_view_item_get_file (item_b),
                                           NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                           FALSE, FALSE);
}

static void
real_set_zoom_level (NautilusListBase *list_base,
                     int               new_level)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (list_base);

    g_return_if_fail (new_level >= computer_view_info.zoom_level_min &&
                      new_level <= computer_view_info.zoom_level_max);

    self->zoom_level = new_level;
    if (g_settings_get_int (nautilus_preferences, "computer-view-zoom-level") != new_level)
    {
        g_settings_set_int (nautilus_preferences, "computer-view-zoom-level", new_level);
    }

    g_object_notify (G_OBJECT (self), "icon-size");
}

/* We only care about the keyboard activation part that GtkListView provides,
 * but we don't need any special filtering here. Indeed, we ask GtkListView
 * to not activate on single click, and we get to handle double clicks before
 * GtkListView does (as one of widget subclassing's goal is to modify the parent
 * class's behavior), while claiming the click gestures, so it means GtkListView
 * will never react to a click event to emit this signal. So we should be pretty
 * safe here with regards to our custom item click handling.
 */
static void
on_list_view_item_activated (GtkListView *list_view,
                             guint        position,
                             gpointer     user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);

    nautilus_list_base_activate_selection (NAUTILUS_LIST_BASE (self), FALSE);
}

static void
real_popup_background_context_menu (NautilusListBase *self,
                                    double            x,
                                    double            y)
{
    g_signal_stop_emission_by_name (G_OBJECT (self), "popup-background-context-menu");
}

static guint
real_get_icon_size (NautilusListBase *list_base_view)
{
    /* Ctrl + scroll and Ctrl + plus/minus: small, medium and large drive icons */
    switch (NAUTILUS_COMPUTER_VIEW (list_base_view)->zoom_level)
    {
        case NAUTILUS_LIST_ZOOM_LEVEL_SMALL:
        {
            return 32;
        }

        case NAUTILUS_LIST_ZOOM_LEVEL_LARGE:
        {
            return 72;
        }

        default:
        {
            return 48;
        }
    }
}

static int
real_get_zoom_level (NautilusListBase *list_base_view)
{
    return NAUTILUS_COMPUTER_VIEW (list_base_view)->zoom_level;
}

static void
real_scroll_to (NautilusListBase   *list_base_view,
                guint               position,
                GtkListScrollFlags  flags,
                GtkScrollInfo      *scroll)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (list_base_view);

    gtk_list_view_scroll_to (self->view_ui, position, flags, scroll);
}

static GVariant *
real_get_sort_state (NautilusListBase *list_base)
{
    return g_variant_take_ref (g_variant_new ("(sb)", "invalid", FALSE));
}

static void
real_set_enable_rubberband (NautilusListBase *list_base,
                            gboolean          enabled)
{
    /* We ignore setting this value as it should always stay disabled. */
}

static void
real_set_sort_state (NautilusListBase *list_base,
                     GVariant         *value)
{
    /* No op */
}

static void
nautilus_computer_view_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (object);

    switch (prop_id)
    {
        case PROP_COLLAPSED_MASK:
        {
            g_value_set_uint (value, self->collapsed_mask);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_computer_view_dispose (GObject *object)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (object);

    g_clear_handle_id (&self->refilter_id, g_source_remove);
    g_clear_object (&self->filter);

    G_OBJECT_CLASS (nautilus_computer_view_parent_class)->dispose (object);
}

static void
nautilus_computer_view_finalize (GObject *object)
{
    G_OBJECT_CLASS (nautilus_computer_view_parent_class)->finalize (object);
}

static void
bind_cell (GtkSignalListItemFactory *factory,
           GtkListItem              *listitem,
           gpointer                  user_data)
{
    GtkWidget *cell = gtk_list_item_get_child (listitem);
    g_autoptr (NautilusViewItem) item = get_view_item (listitem);

    nautilus_view_item_set_item_ui (item, cell);
}

static void
unbind_cell (GtkSignalListItemFactory *factory,
             GtkListItem              *listitem,
             gpointer                  user_data)
{
    g_autoptr (NautilusViewItem) item = get_view_item (listitem);

    /* item may be NULL when row has just been destroyed. */
    if (item != NULL)
    {
        nautilus_view_item_set_item_ui (item, NULL);
    }
}

static void
setup_cell (GtkSignalListItemFactory *factory,
            GtkListItem              *listitem,
            gpointer                  user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);
    NautilusViewCell *cell;
    GtkExpression *expression;

    cell = nautilus_computer_cell_new (NAUTILUS_LIST_BASE (self));
    gtk_list_item_set_child (listitem, GTK_WIDGET (cell));
    setup_cell_common (G_OBJECT (listitem), cell, GTK_WIDGET (cell));

    g_object_bind_property (self, "icon-size",
                            cell, "icon-size",
                            G_BINDING_SYNC_CREATE);

    /* Use file display name as accessible label. Explaining in pseudo-code:
     * listitem:accessible-name :- listitem:item:item:file:display-name */
    expression = gtk_property_expression_new (GTK_TYPE_LIST_ITEM, NULL, "item");
    expression = gtk_property_expression_new (GTK_TYPE_TREE_LIST_ROW, expression, "item");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_VIEW_ITEM, expression, "file");
    expression = gtk_property_expression_new (NAUTILUS_TYPE_FILE, expression, "display-name");
    gtk_expression_bind (expression, listitem, "accessible-label", listitem);
}

static const char *
section_title (gint section)
{
    switch (section)
    {
        case SECTION_NETWORK:
        {
            return _("Network Addresses");
        }

        case SECTION_EXTERNAL:
        {
            return _("External Drives");
        }

        default:
        {
            return _("Devices and Drives");
        }
    }
}

static void
update_header_arrow (GtkWidget *button,
                     gboolean   collapsed)
{
    GtkWidget *box = gtk_button_get_child (GTK_BUTTON (button));
    GtkWidget *arrow = gtk_widget_get_first_child (box);

    gtk_image_set_from_icon_name (GTK_IMAGE (arrow), collapsed ? "pan-end-symbolic" : "pan-down-symbolic");
}

static void
on_header_clicked (GtkButton *button,
                   gpointer   user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);
    gint section = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "wdn-section"));

    self->collapsed_mask ^= (1u << section);
    update_header_arrow (GTK_WIDGET (button), (self->collapsed_mask & (1u << section)) != 0);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLLAPSED_MASK]);
}

static void
bind_header (GtkSignalListItemFactory *factory,
             GtkListHeader            *listheader,
             gpointer                  user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);
    GtkWidget *button = gtk_list_header_get_child (listheader);
    GtkWidget *box = gtk_button_get_child (GTK_BUTTON (button));
    GtkWidget *label = gtk_widget_get_last_child (box);
    GtkTreeListRow *row = GTK_TREE_LIST_ROW (gtk_list_header_get_item (listheader));
    g_autoptr (NautilusViewItem) item = NAUTILUS_VIEW_ITEM (gtk_tree_list_row_get_item (row));
    gint section = get_section (item);

    g_object_set_data (G_OBJECT (button), "wdn-section", GINT_TO_POINTER (section));
    gtk_label_set_label (GTK_LABEL (label), section_title (section));
    update_header_arrow (button, (self->collapsed_mask & (1u << section)) != 0);
}

static void
setup_header (GtkSignalListItemFactory *factory,
              GtkListHeader            *listheader,
              gpointer                  user_data)
{
    GtkWidget *button = gtk_button_new ();
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
    GtkWidget *label = gtk_label_new (NULL);

    gtk_widget_add_css_class (button, "flat");
    gtk_widget_add_css_class (label, "heading");
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_set_halign (button, GTK_ALIGN_START);
    gtk_box_append (GTK_BOX (box), arrow);
    gtk_box_append (GTK_BOX (box), label);
    gtk_button_set_child (GTK_BUTTON (button), box);
    g_signal_connect (button, "clicked", G_CALLBACK (on_header_clicked), user_data);

    gtk_list_header_set_child (listheader, button);
}

/* The extension sets "wdn_hide" = "1" on the drive of the system disk: it is shown once, as the root filesystem item
 * ("<disk name> (System disk)"), so there is no second entry that cannot be mounted. */
static gboolean
computer_item_visible (gpointer item,
                       gpointer user_data)
{
    static GQuark hide_q = 0;
    NautilusFile *file = nautilus_view_item_get_file (NAUTILUS_VIEW_ITEM (item));
    g_autofree char *hide = NULL;

    if (hide_q == 0)
    {
        hide_q = g_quark_from_static_string ("wdn_hide");
    }

    hide = nautilus_file_get_string_attribute_q (file, hide_q);

    return !(hide != NULL && hide[0] == '1');
}

static gboolean
on_refilter (gpointer user_data)
{
    NautilusComputerView *self = NAUTILUS_COMPUTER_VIEW (user_data);

    self->refilter_id = 0;
    if (self->filter != NULL)
    {
        gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);
    }

    /* A second look a little later, for attributes the extension sets after the first round */
    if (self->refilter_rounds > 0)
    {
        self->refilter_rounds--;
        self->refilter_id = g_timeout_add (1000, on_refilter, self);
    }

    return G_SOURCE_REMOVE;
}

static void
on_items_changed (NautilusComputerView *self)
{
    if (self->refilter_id == 0)
    {
        self->refilter_rounds = 2;
        self->refilter_id = g_timeout_add (250, on_refilter, self);
    }
}

static void
on_model_changed (NautilusComputerView *self)
{
    NautilusViewModel *model = nautilus_list_base_get_model (NAUTILUS_LIST_BASE (self));

    if (model != NULL)
    {
        g_clear_object (&self->filter);
        self->filter = GTK_FILTER (gtk_custom_filter_new (computer_item_visible, NULL, NULL));
        nautilus_view_model_set_filter (model, self->filter);
        /* The extension fills in its attributes a moment after an item is added: look again when items change */
        g_signal_connect_swapped (model, "items-changed", G_CALLBACK (on_items_changed), self);
        g_autoptr (GtkCustomSorter) sorter = gtk_custom_sorter_new (sort_computer_items, self, NULL);
        g_autoptr (GtkCustomSorter) sections_sorter = gtk_custom_sorter_new (sort_computer_sections, self, NULL);

        nautilus_view_model_set_sorter (model, GTK_SORTER (sorter));
        nautilus_view_model_set_section_sorter (model, GTK_SORTER (sections_sorter));
    }

    gtk_list_view_set_model (self->view_ui, GTK_SELECTION_MODEL (model));
}

static GtkListView *
create_view_ui (NautilusComputerView *self)
{
    g_autoptr (GtkListItemFactory) factory = gtk_signal_list_item_factory_new ();
    g_autoptr (GtkListItemFactory) header_factory = gtk_signal_list_item_factory_new ();
    GtkListView *list_view;

    g_signal_connect (factory, "setup", G_CALLBACK (setup_cell), self);
    g_signal_connect (factory, "bind", G_CALLBACK (bind_cell), self);
    g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cell), self);

    list_view = GTK_LIST_VIEW (gtk_list_view_new (NULL, g_steal_pointer (&factory)));

    g_signal_connect (header_factory, "setup", G_CALLBACK (setup_header), self);
    g_signal_connect (header_factory, "bind", G_CALLBACK (bind_header), self);
    gtk_list_view_set_header_factory (list_view, header_factory);

    /* We don't use the built-in child activation feature for clicks because it
     * doesn't fill all our needs nor does it match our expected behavior.
     * Instead, we roll our own event handling and double/single click mode.
     * However, GtkListView:single-click-activate has other effects besides
     * activation, as it affects the selection behavior as well (e.g. selects on
     * hover). Setting it to FALSE gives us the expected behavior. */
    gtk_list_view_set_single_click_activate (list_view, FALSE);
    gtk_list_view_set_enable_rubberband (list_view, FALSE);
    gtk_list_view_set_tab_behavior (list_view, GTK_LIST_TAB_ITEM);

    gtk_accessible_update_property (GTK_ACCESSIBLE (list_view),
                                    GTK_ACCESSIBLE_PROPERTY_LABEL,
                                    _("Content View"),
                                    GTK_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION,
                                    _("View of the current location"),
                                    -1);

    /* While we don't want to use GTK's click activation, we'll let it handle
     * the key activation part (with Enter). */
    g_signal_connect (list_view, "activate", G_CALLBACK (on_list_view_item_activated), self);

    return list_view;
}

static void
nautilus_computer_view_class_init (NautilusComputerViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    NautilusListBaseClass *list_base_view_class = NAUTILUS_LIST_BASE_CLASS (klass);

    object_class->get_property = nautilus_computer_view_get_property;
    object_class->dispose = nautilus_computer_view_dispose;

    properties[PROP_COLLAPSED_MASK] = g_param_spec_uint ("collapsed-mask", NULL, NULL,
                                                         0, G_MAXUINT, 0,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_properties (object_class, N_PROPS, properties);
    object_class->finalize = nautilus_computer_view_finalize;

    list_base_view_class->get_icon_size = real_get_icon_size;
    list_base_view_class->get_sort_state = real_get_sort_state;
    list_base_view_class->get_view_info = real_get_view_info;
    list_base_view_class->get_zoom_level = real_get_zoom_level;
    list_base_view_class->popup_background_context_menu = real_popup_background_context_menu;
    list_base_view_class->scroll_to = real_scroll_to;
    list_base_view_class->set_enable_rubberband = real_set_enable_rubberband;
    list_base_view_class->set_sort_state = real_set_sort_state;
    list_base_view_class->set_zoom_level = real_set_zoom_level;
}

static void
ensure_css (void)
{
    static gboolean done = FALSE;
    GtkCssProvider *provider;

    if (done || gdk_display_get_default () == NULL)
    {
        return;
    }
    done = TRUE;

    provider = gtk_css_provider_new ();
    gtk_css_provider_load_from_string (provider,
                                       ".nautilus-computer-view row.wdn-collapsed { min-height: 0; padding: 0; margin: 0; }\n"
                                       ".nautilus-computer-view levelbar.wdn-drive-bar block.filled { background-color: @accent_bg_color; }\n"
                                       ".nautilus-computer-view levelbar.wdn-drive-bar.wdn-full block.filled { background-color: @error_bg_color; }\n"
                                       ".nautilus-computer-view levelbar.wdn-drive-bar block { min-height: 12px; border-radius: 6px; }\n");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider),
                                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);
}

static void
nautilus_computer_view_init (NautilusComputerView *self)
{
    GtkWidget *scrolled_window = nautilus_list_base_get_scrolled_window (NAUTILUS_LIST_BASE (self));

    ensure_css ();
    gtk_widget_add_css_class (GTK_WIDGET (self), "nautilus-computer-view");

    self->view_ui = create_view_ui (self);
    nautilus_list_base_setup_gestures (NAUTILUS_LIST_BASE (self));

    g_signal_connect_swapped (self, "notify::model", G_CALLBACK (on_model_changed), self);

    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolled_window),
                                   GTK_WIDGET (self->view_ui));
    nautilus_list_base_set_zoom_level (NAUTILUS_LIST_BASE (self),
                                       CLAMP (g_settings_get_int (nautilus_preferences, "computer-view-zoom-level"),
                                              computer_view_info.zoom_level_min, computer_view_info.zoom_level_max));
}

NautilusComputerView *
nautilus_computer_view_new (void)
{
    return g_object_new (NAUTILUS_TYPE_COMPUTER_VIEW, NULL);
}
