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
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gthumb.h>
#include "actions.h"
#include "callbacks.h"
#include "dlg-photo-importer.h"
#include "photo-importer.h"
#include "preferences.h"


static const GActionEntry actions[] = {
	{ "import-device", gth_browser_activate_import_device },
	{ "import-folder", gth_browser_activate_import_folder }
};


static const GthMenuEntry action_entries[] = {
	{ N_("_Removable Device…"), "win.import-device", NULL, "camera-photo-symbolic" },
	{ N_("F_older…"), "win.import-folder", NULL, "folder-symbolic" }
};


void
pi__gth_browser_construct_cb (GthBrowser *browser)
{
	GtkBuilder	*builder;
	GMenuModel	*import_menu;
	GMenu		*other_actions;

	g_return_if_fail (GTH_IS_BROWSER (browser));

	g_action_map_add_action_entries (G_ACTION_MAP (browser),
					 actions,
					 G_N_ELEMENTS (actions),
					 browser);

	builder = gtk_builder_new_from_resource ("/org/gnome/gThumb/photo_importer/data/ui/import-menu.ui");
	import_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "import-menu"));
	other_actions = gth_menu_manager_get_menu (gth_browser_get_menu_manager (browser, GTH_BROWSER_MENU_MANAGER_GEARS_OTHER_ACTIONS));
	g_menu_append_submenu (other_actions, _("I_mport From"), import_menu);

	gth_browser_add_menu_manager_for_menu (browser, GTH_BROWSER_MENU_MANAGER_WEB_IMPORTERS, G_MENU (gtk_builder_get_object (builder, "web-importers")));
	gth_browser_add_menu_manager_for_menu (browser, GTH_BROWSER_MENU_MANAGER_OTHER_IMPORTERS, G_MENU (gtk_builder_get_object (builder, "other-importers")));

	gth_menu_manager_append_entries (gth_browser_get_menu_manager (browser, GTH_BROWSER_MENU_MANAGER_OTHER_IMPORTERS),
				       	 action_entries,
				       	 G_N_ELEMENTS (action_entries));

	g_object_unref (builder);

}


/* -- pi__import_photos_cb -- */


typedef struct {
	GthBrowser *browser;
	GFile      *source;
} ImportData;


static void
import_data_unref (gpointer user_data)
{
	ImportData *data = user_data;

	g_object_unref (data->browser);
	_g_object_unref (data->source);
	g_free (data);
}


static gboolean
import_photos_idle_cb (gpointer user_data)
{
	ImportData *data = user_data;

	dlg_photo_importer_from_device (data->browser, data->source);

	return FALSE;
}


void
pi__import_photos_cb (GthBrowser *browser,
		      GFile      *source)
{
	ImportData *data;

	data = g_new0 (ImportData, 1);
	data->browser = g_object_ref (browser);
	data->source = _g_object_ref (source);
	g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
			 import_photos_idle_cb,
			 data,
			 import_data_unref);
}


/* -- pi__dlg_preferences_construct_cb -- */


#define PREFERENCES_DATA_KEY "photo-import-preference-data"


typedef struct {
	GtkBuilder *builder;
	GSettings  *settings;
} PreferencesData;


static void
preferences_data_free (PreferencesData *data)
{
	g_object_unref (data->settings);
	g_object_unref (data->builder);
	g_free (data);
}


static void
adjust_orientation_checkbutton_toggled_cb (GtkToggleButton *button,
					   PreferencesData *data)
{
	g_settings_set_boolean (data->settings, PREF_PHOTO_IMPORTER_ADJUST_ORIENTATION, gtk_toggle_button_get_active (button));
}


void
pi__dlg_preferences_construct_cb (GtkWidget  *dialog,
				  GthBrowser *browser,
				  GtkBuilder *dialog_builder)
{
	PreferencesData *data;
	GtkWidget       *general_vbox;
	GtkWidget       *importer_options;
	GtkWidget       *widget;

	data = g_new0 (PreferencesData, 1);
	data->builder = gtk_builder_new_from_resource ("/org/gnome/gThumb/photo_importer/data/ui/photo-importer-options.ui");
	data->settings = g_settings_new (GTHUMB_PHOTO_IMPORTER_SCHEMA);

	general_vbox = _gtk_builder_get_widget (dialog_builder, "general_page");
	importer_options = _gtk_builder_get_widget (data->builder, "importer_options");
	gtk_box_pack_start (GTK_BOX (general_vbox),
			    importer_options,
			    FALSE,
			    FALSE,
			    0);
	/* move the options before the 'other' options */
	gtk_box_reorder_child (GTK_BOX (general_vbox),
			       importer_options,
			       _gtk_container_get_n_children (GTK_CONTAINER (general_vbox)) - 2);

	widget = _gtk_builder_get_widget (data->builder, "adjust_orientation_checkbutton");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), g_settings_get_boolean (data->settings, PREF_PHOTO_IMPORTER_ADJUST_ORIENTATION));
	g_signal_connect (widget,
			  "toggled",
			  G_CALLBACK (adjust_orientation_checkbutton_toggled_cb),
			  data);

	g_object_set_data_full (G_OBJECT (dialog), PREFERENCES_DATA_KEY, data, (GDestroyNotify) preferences_data_free);
}
