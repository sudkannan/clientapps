/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2009 Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <gtk/gtk.h>
#include "gth-main.h"
#include "gth-file-tool.h"
#include "gth-toolbox.h"
#include "gtk-utils.h"


#define GTH_TOOLBOX_PAGE_TOOLS "GthToolbox.Tools"
#define GTH_TOOLBOX_PAGE_OPTIONS "GthToolbox.Options"
#define GRID_COLUMNS 4
#define GRID_SPACING 20


enum  {
	GTH_TOOLBOX_DUMMY_PROPERTY,
	GTH_TOOLBOX_NAME
};


struct _GthToolboxPrivate {
	char      *name;
	GtkWidget *tool_grid[GTH_TOOLBOX_N_SECTIONS];
	GtkWidget *options;
	GtkWidget *options_icon;
	GtkWidget *options_title;
	GtkWidget *active_tool;
};


G_DEFINE_TYPE (GthToolbox, gth_toolbox, GTK_TYPE_STACK)


static void
gth_toolbox_set_name (GthToolbox *self,
		      const char *name)
{
	g_free (self->priv->name);
	self->priv->name = NULL;

	if (name != NULL)
		self->priv->name = g_strdup (name);
}


static void
gth_toolbox_set_property (GObject      *object,
			  guint         property_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	GthToolbox *self;

	self = GTH_TOOLBOX (object);

	switch (property_id) {
	case GTH_TOOLBOX_NAME:
		gth_toolbox_set_name (self, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void
gth_toolbox_finalize (GObject *object)
{
	GthToolbox *self;

	self = GTH_TOOLBOX (object);

	g_free (self->priv->name);

	G_OBJECT_CLASS (gth_toolbox_parent_class)->finalize (object);
}


static void
gth_toolbox_class_init (GthToolboxClass *klass)
{
	GObjectClass *gobject_class;

	g_type_class_add_private (klass, sizeof (GthToolboxPrivate));

	gobject_class = (GObjectClass*) klass;
	gobject_class->finalize = gth_toolbox_finalize;
	gobject_class->set_property = gth_toolbox_set_property;

	g_object_class_install_property (G_OBJECT_CLASS (klass),
					 GTH_TOOLBOX_NAME,
					 g_param_spec_string ("name",
							      "name",
							      "name",
							      NULL,
							      G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}


static const char * section_title[] = {
	NULL,
	N_("Colors"),
	N_("Rotation"),
	N_("Format")
};


static void
gth_toolbox_init (GthToolbox *toolbox)
{
	GtkWidget *scrolled;
	int        i;
	GtkWidget *grid_box;
	GtkWidget *options_box;
	GtkWidget *options_header;
	GtkWidget *header_align;

	toolbox->priv = G_TYPE_INSTANCE_GET_PRIVATE (toolbox, GTH_TYPE_TOOLBOX, GthToolboxPrivate);

	/* tool list page */

	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_NONE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scrolled);
	gtk_stack_add_named (GTK_STACK (toolbox), scrolled, GTH_TOOLBOX_PAGE_TOOLS);

	grid_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add (GTK_CONTAINER (scrolled), grid_box);
	gtk_widget_set_margin_top (grid_box, GRID_SPACING * 2);
	gtk_widget_set_halign (grid_box, GTK_ALIGN_CENTER);

	for (i = 0; i < GTH_TOOLBOX_N_SECTIONS; i++) {
		GtkWidget *box;

		if (i > 0) {
			PangoAttrList *attrs;
			GtkWidget     *label;

			attrs = pango_attr_list_new ();
			pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

			label = gtk_label_new (_(section_title[i]));
			gtk_label_set_attributes (GTK_LABEL (label), attrs);
			gtk_widget_set_halign (label, GTK_ALIGN_START);
			gtk_widget_set_margin_bottom (label, GRID_SPACING);
			gtk_box_pack_start (GTK_BOX (grid_box), label, FALSE, FALSE, 0);

			pango_attr_list_unref (attrs);
		}
		toolbox->priv->tool_grid[i] = gtk_grid_new ();
		gtk_widget_set_margin_bottom (toolbox->priv->tool_grid[i], GRID_SPACING);
		gtk_style_context_add_class (gtk_widget_get_style_context (toolbox->priv->tool_grid[i]), "toolbox");
		gtk_grid_set_row_spacing (GTK_GRID (toolbox->priv->tool_grid[i]), GRID_SPACING);
		gtk_grid_set_column_spacing (GTK_GRID (toolbox->priv->tool_grid[i]), GRID_SPACING);
		gtk_grid_set_column_homogeneous (GTK_GRID (toolbox->priv->tool_grid[i]), TRUE);
		gtk_grid_set_row_homogeneous (GTK_GRID (toolbox->priv->tool_grid[i]), TRUE);

		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_pack_start (GTK_BOX (box), toolbox->priv->tool_grid[i], FALSE, FALSE, 0);

		gtk_box_pack_start (GTK_BOX (grid_box), box, FALSE, FALSE, 0);
	}
	gtk_widget_show_all (grid_box);

	/* tool options page */

	options_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (options_box);
	gtk_stack_add_named (GTK_STACK (toolbox), options_box, GTH_TOOLBOX_PAGE_OPTIONS);

	header_align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (header_align), 5, 5, 0, 0);

	options_header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (options_header);
	gtk_container_add (GTK_CONTAINER (header_align), options_header);

	toolbox->priv->options_icon = gtk_image_new ();
	gtk_widget_show (toolbox->priv->options_icon);
	gtk_box_pack_start (GTK_BOX (options_header), toolbox->priv->options_icon, FALSE, FALSE, 0);

	toolbox->priv->options_title = gtk_label_new ("");
	gtk_label_set_use_markup (GTK_LABEL (toolbox->priv->options_title), TRUE);
	gtk_widget_show (toolbox->priv->options_title);
	gtk_box_pack_start (GTK_BOX (options_header), toolbox->priv->options_title, FALSE, FALSE, 0);

	gtk_widget_show (header_align);
	gtk_box_pack_start (GTK_BOX (options_box), header_align, FALSE, FALSE, 0);

	toolbox->priv->options = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (toolbox->priv->options), GTK_SHADOW_IN);
	gtk_widget_show (toolbox->priv->options);
	gtk_box_pack_start (GTK_BOX (options_box), toolbox->priv->options, TRUE, TRUE, 0);
}


static void
child_clicked_cb (GtkWidget *tool,
		  gpointer   data)
{
	gth_file_tool_activate (GTH_FILE_TOOL (tool));
}


static void
child_show_options_cb (GtkWidget *tool,
		       gpointer   data)
{
	GthToolbox *toolbox = data;
	GtkWidget  *options;
	char       *markup;

	options = gth_file_tool_get_options (GTH_FILE_TOOL (tool));
	if (options == NULL)
		return;

	toolbox->priv->active_tool = tool;

	_gtk_container_remove_children (GTK_CONTAINER (toolbox->priv->options), NULL, NULL);

	markup = g_markup_printf_escaped ("<span size='large' weight='bold'>%s</span>", gth_file_tool_get_options_title (GTH_FILE_TOOL (tool)));
	gtk_label_set_markup (GTK_LABEL (toolbox->priv->options_title), markup);
	gtk_image_set_from_icon_name (GTK_IMAGE (toolbox->priv->options_icon), gth_file_tool_get_icon_name (GTH_FILE_TOOL (tool)), GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (toolbox->priv->options), options);
	gtk_stack_set_visible_child_name (GTK_STACK (toolbox), GTH_TOOLBOX_PAGE_OPTIONS);

	g_free (markup);
}


static void
child_hide_options_cb (GtkWidget *tool,
		       gpointer   data)
{
	GthToolbox *toolbox = data;
	GtkWidget  *tool_options;

	gth_file_tool_destroy_options (GTH_FILE_TOOL (tool));
	tool_options = gtk_bin_get_child (GTK_BIN (toolbox->priv->options));
	if (tool_options != NULL)
		gtk_container_remove (GTK_CONTAINER (toolbox->priv->options), tool_options);
	gtk_stack_set_visible_child_name (GTK_STACK (toolbox), GTH_TOOLBOX_PAGE_TOOLS);
	gth_toolbox_update_sensitivity (GTH_TOOLBOX (toolbox));

	toolbox->priv->active_tool = NULL;
}


static void
_gth_toolbox_add_children (GthToolbox *toolbox)
{
	GArray *children;
	int     i;
	int     row[GTH_TOOLBOX_N_SECTIONS];
	int     column[GTH_TOOLBOX_N_SECTIONS];

	children = gth_main_get_type_set (toolbox->priv->name);
	if (children == NULL)
		return;

	for (i = 0; i < GTH_TOOLBOX_N_SECTIONS; i++) {
		row[i] = 0;
		column[i] = -1;
	}

	for (i = 0; i < children->len; i++) {
		GType      child_type;
		GtkWidget *child;
		int        section;

		child_type = g_array_index (children, GType, i);
		child = g_object_new (child_type, NULL);
		g_signal_connect (child, "clicked", G_CALLBACK (child_clicked_cb), toolbox);
		g_signal_connect (child, "show-options", G_CALLBACK (child_show_options_cb), toolbox);
		g_signal_connect (child, "hide-options", G_CALLBACK (child_hide_options_cb), toolbox);
		gtk_widget_show (child);

		section = gth_file_tool_get_section (GTH_FILE_TOOL (child));
		column[section] = column[section] + 1;
		if (column[section] == GRID_COLUMNS) {
			column[section] = 0;
			row[section] = row[section] + 1;
		}

		gtk_grid_attach (GTK_GRID (toolbox->priv->tool_grid[section]),
				 child,
				 column[section], row[section],
				 1, 1);
	}
}


GtkWidget *
gth_toolbox_new (const char *name)
{
	GtkWidget *toolbox;

	toolbox = g_object_new (GTH_TYPE_TOOLBOX, "name", name, NULL);
	_gth_toolbox_add_children (GTH_TOOLBOX (toolbox));

	return toolbox;
}


void
gth_toolbox_update_sensitivity (GthToolbox *toolbox)
{
	int i;

	for (i = 0; i < GTH_TOOLBOX_N_SECTIONS; i++) {
		GList *children;
		GList *scan;

		children = gtk_container_get_children (GTK_CONTAINER (toolbox->priv->tool_grid[i]));
		for (scan = children; scan; scan = scan->next) {
			GtkWidget *child = scan->data;

			if (! GTH_IS_FILE_TOOL (child))
				continue;

			gth_file_tool_update_sensitivity (GTH_FILE_TOOL (child));
		}

		g_list_free (children);
	}
}


void
gth_toolbox_deactivate_tool (GthToolbox *toolbox)
{
	if (toolbox->priv->active_tool != NULL)
		gth_file_tool_cancel (GTH_FILE_TOOL (toolbox->priv->active_tool));
}


gboolean
gth_toolbox_tool_is_active (GthToolbox *toolbox)
{
	return toolbox->priv->active_tool != NULL;
}


GtkWidget *
gth_toolbox_get_tool (GthToolbox *toolbox,
		      GType       tool_type)
{
	GtkWidget *tool = NULL;
	int        i;

	for (i = 0; (tool == NULL) && (i < GTH_TOOLBOX_N_SECTIONS); i++) {
		GList *children;
		GList *scan;

		children = gtk_container_get_children (GTK_CONTAINER (toolbox->priv->tool_grid[i]));
		for (scan = children; scan; scan = scan->next) {
			GtkWidget *child = scan->data;

			if (G_OBJECT_TYPE (child) == tool_type) {
				tool = child;
				break;
			}
		}

		g_list_free (children);
	}

	return tool;
}
