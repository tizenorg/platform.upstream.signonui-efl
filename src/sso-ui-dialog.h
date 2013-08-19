/*
 * This file is part of signonui-efl
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Author: Jussi Kukkonen <jussi.kukkonen@intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __SSO_UI_DIALOG_H__
#define __SSO_UI_DIALOG_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <gsignond/gsignond-signonui-data.h>

G_BEGIN_DECLS

#define SSO_TYPE_UI_DIALOG sso_ui_dialog_get_type()

#define SSO_UI_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SSO_TYPE_UI_DIALOG, SSOUIDialog))

#define SSO_UI_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SSO_TYPE_UI_DIALOG, SSOUIDialogClass))

#define SSO_IS_UI_DIALOG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SSO_TYPE_UI_DIALOG))

#define SSO_IS_UI_DIALOG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SSO_TYPE_UI_DIALOG))

#define SSO_UI_DIALOG_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SSO_TYPE_UI_DIALOG, SSOUIDialogClass))

typedef struct _SSOUIDialogPrivate SSOUIDialogPrivate;

typedef struct _SSOUIDialog {
  GObject parent;
  SSOUIDialogPrivate *priv;
} SSOUIDialog;

typedef struct _SSOUIDialogClass {
  GObjectClass parent_class;
  void (* closed) (SSOUIDialog *dialog);
} SSOUIDialogClass;

GType sso_ui_dialog_get_type (void) G_GNUC_CONST;

SSOUIDialog*           sso_ui_dialog_new              (GSignondSignonuiData *parameters, GDBusMethodInvocation *invocation);

void                   sso_ui_dialog_set_parameters   (SSOUIDialog *self, GSignondSignonuiData *parameters);
gboolean               sso_ui_dialog_refresh_captcha  (SSOUIDialog * self, const gchar *url);

GDBusMethodInvocation* sso_ui_dialog_get_invocation   (SSOUIDialog *dialog);
const gchar*           sso_ui_dialog_get_request_id   (SSOUIDialog *dialog);
GSignondSignonuiData*  sso_ui_dialog_get_return_value (SSOUIDialog *dialog);

gboolean               sso_ui_dialog_show             (SSOUIDialog *dialog);
void                   sso_ui_dialog_close            (SSOUIDialog *dialog);

G_END_DECLS

#endif /* __SSO_UI_DIALOG_H__ */
