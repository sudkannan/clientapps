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

#ifndef GTH_SCRIPT_H
#define GTH_SCRIPT_H

#include <glib-object.h>
#include <gthumb.h>

G_BEGIN_DECLS

#define GTH_TYPE_SCRIPT         (gth_script_get_type ())
#define GTH_SCRIPT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTH_TYPE_SCRIPT, GthScript))
#define GTH_SCRIPT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTH_TYPE_SCRIPT, GthScriptClass))
#define GTH_IS_SCRIPT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTH_TYPE_SCRIPT))
#define GTH_IS_SCRIPT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTH_TYPE_SCRIPT))
#define GTH_SCRIPT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GTH_TYPE_SCRIPT, GthScriptClass))

typedef struct _GthScript         GthScript;
typedef struct _GthScriptPrivate  GthScriptPrivate;
typedef struct _GthScriptClass    GthScriptClass;


struct _GthScript
{
	GObject __parent;
	GthScriptPrivate *priv;
};

struct _GthScriptClass
{
	GObjectClass __parent_class;
};

GType             gth_script_get_type                  (void) G_GNUC_CONST;
GthScript *       gth_script_new                       (void);
const char *      gth_script_get_id                    (GthScript  *script);
const char *      gth_script_get_display_name          (GthScript  *script);
const char *      gth_script_get_command               (GthScript  *script);
gboolean          gth_script_is_visible                (GthScript  *script);
gboolean          gth_script_is_shell_script           (GthScript  *script);
gboolean          gth_script_for_each_file             (GthScript  *script);
gboolean          gth_script_wait_command              (GthScript  *script);
char *            gth_script_get_requested_attributes  (GthScript  *script);
char *            gth_script_get_command_line          (GthScript  *script,
						        GtkWindow  *parent,
						        GList      *file_list /* GthFileData */,
						        GError    **error);
guint             gth_script_get_shortcut              (GthScript  *script);

G_END_DECLS

#endif /* GTH_SCRIPT_H */
