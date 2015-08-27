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
#include <math.h>
#include <gthumb.h>
#include <extensions/image_viewer/image-viewer.h>
#include "gth-file-tool-flip.h"


G_DEFINE_TYPE (GthFileToolFlip, gth_file_tool_flip, GTH_TYPE_IMAGE_VIEWER_PAGE_TOOL)


static gpointer
flip_exec (GthAsyncTask *task,
	   gpointer      user_data)
{
	cairo_surface_t *source;
	cairo_surface_t *destination;

	source = gth_image_task_get_source_surface (GTH_IMAGE_TASK (task));
	destination = _cairo_image_surface_transform (source, GTH_TRANSFORM_FLIP_V);
	gth_image_task_set_destination_surface (GTH_IMAGE_TASK (task), destination);

	cairo_surface_destroy (destination);
	cairo_surface_destroy (source);

	return NULL;
}


static void
gth_file_tool_flip_activate (GthFileTool *base)
{
	GtkWidget *window;
	GtkWidget *viewer_page;
	GthTask   *task;

	window = gth_file_tool_get_window (base);
	viewer_page = gth_browser_get_viewer_page (GTH_BROWSER (window));
	if (! GTH_IS_IMAGE_VIEWER_PAGE (viewer_page))
		return;

	task = gth_image_viewer_task_new (GTH_IMAGE_VIEWER_PAGE (viewer_page),
					  _("Applying changes"),
					  NULL,
					  flip_exec,
					  NULL,
					  NULL,
					  NULL);
	g_signal_connect (task,
			  "completed",
			  G_CALLBACK (gth_image_viewer_task_set_destination),
			  NULL);
	gth_browser_exec_task (GTH_BROWSER (window), task, FALSE);
}


static void
gth_file_tool_flip_class_init (GthFileToolFlipClass *klass)
{
	GthFileToolClass *file_tool_class;

	file_tool_class = GTH_FILE_TOOL_CLASS (klass);
	file_tool_class->activate = gth_file_tool_flip_activate;
}


static void
gth_file_tool_flip_init (GthFileToolFlip *self)
{
	gth_file_tool_construct (GTH_FILE_TOOL (self), "image-flip-vertical-symbolic", _("Flip"), GTH_TOOLBOX_SECTION_ROTATION);
	gtk_widget_set_tooltip_text (GTK_WIDGET (self), _("Flip the image vertically"));
}
