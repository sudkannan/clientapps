/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  GThumb
 *
 *  Copyright (C) 2010 Free Software Foundation, Inc.
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
#include <extensions/list_tools/list-tools.h>
#include "actions.h"
#include "callbacks.h"


static const GActionEntry actions[] = {
	{ "convert-format", gth_browser_activate_convert_format }
};


static const GthMenuEntry action_entries[] = {
	{ N_("Convert Format…"), "win.convert-format", NULL, "convert-format-symbolic" }
};


void
cf__gth_browser_construct_cb (GthBrowser *browser)
{
	g_return_if_fail (GTH_IS_BROWSER (browser));

	g_action_map_add_action_entries (G_ACTION_MAP (browser),
					 actions,
					 G_N_ELEMENTS (actions),
					 browser);
	gth_menu_manager_append_entries (gth_browser_get_menu_manager (browser, GTH_BROWSER_MENU_MANAGER_TOOLS),
					 action_entries,
					 G_N_ELEMENTS (action_entries));
}


void
cf__gth_browser_update_sensitivity_cb (GthBrowser *browser)
{
	int      n_selected;
	gboolean sensitive;

	n_selected = gth_file_selection_get_n_selected (GTH_FILE_SELECTION (gth_browser_get_file_list_view (browser)));
	sensitive = n_selected > 0;
	gth_window_enable_action (GTH_WINDOW (browser), "convert-format", sensitive);
}
