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

#include "config.h"
#include <stdlib.h>
#include <Elementary.h>
#ifdef USE_WEBKIT
#include <ewk_view.h>
#else
#include <Ecore.h>
#include <libsoup/soup.h>
#endif // USE_WEBKIT
#include <gsignond/gsignond-signonui-data.h>

#include "sso-ui-dialog.h"
/*
   FIXME : below defs should be remove once we revert 
   change in gsignond
*/
#ifndef gsignond_signonui_data_ref
#   define gsignond_signonui_data_ref gsignond_dictionary_ref
#endif
#ifndef gsignond_signonui_data_unref
#   define gsignond_signonui_data_unref gsignond_dictionary_unref
#endif
#ifndef gsignond_signonui_data_new
#   define gsignond_signonui_data_new gsignond_dictionary_new
#endif

G_DEFINE_TYPE (SSOUIDialog, sso_ui_dialog, G_TYPE_OBJECT)

#define UI_DIALOG_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SSO_TYPE_UI_DIALOG, SSOUIDialogPrivate))

#ifndef USE_WEBKIT
extern char *browser_cmd; // sso-ui.c : command line switch
extern unsigned int http_port; // sso-ui.c: comnand line switch

static gboolean start_http_server(SSOUIDialog *self);
static gboolean start_browser_exe(SSOUIDialog *self, const gchar *url);
static void stop_http_server(SSOUIDialog *self);
static void stop_browser_exe(SSOUIDialog *self);
#endif // USE_WEBKIT

struct _SSOUIDialogPrivate {
  GDBusMethodInvocation *invocation;
  GSignondSignonuiData *params;
  const gchar *old_password;
  const gchar *oauth_final_url;

  int error_code;
#if 0
  /* hack for automated testing: contains values that should be sent as reply */
  char *test_reply;
#endif
  gchar *oauth_response;

  Evas_Object *dialog;
  Evas_Object *message;
  Evas_Object *username_entry;
  Evas_Object *password_entry;
  Evas_Object *password_confirm1_entry;
  Evas_Object *password_confirm2_entry;
  Evas_Object *remember_check;
  Evas_Object *captcha_image;
  Evas_Object *captcha_entry;
#ifdef USE_WEBKIT
  Evas_Object *oauth_web;
#else
  Ecore_Exe *browser_exe;
  Ecore_Event_Handler *browser_event_handler;
  SoupServer *http_server;
#endif // USE_WEBKIT
};

enum {
  PROP_0,
  PROP_INVOCATION,
  PROP_PARAMETERS,
  PROP_RETURN_VALUE,
};

enum {
  CLOSED_SIGNAL,
  LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static void
sso_ui_dialog_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  SSOUIDialog *self = SSO_UI_DIALOG (object);

  switch (property_id)
    {
    case PROP_INVOCATION:
      g_value_set_object (value, self->priv->invocation);
      break;
    case PROP_RETURN_VALUE:
      g_value_set_boxed (value, sso_ui_dialog_get_return_value (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sso_ui_dialog_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  SSOUIDialog *self = SSO_UI_DIALOG (object);
  switch (property_id)
    {
    case PROP_INVOCATION:
      self->priv->invocation = g_value_get_object (value);
      break;
    case PROP_PARAMETERS: {
      GSignondSignonuiData *params = g_value_get_boxed (value);
      sso_ui_dialog_set_parameters (self, params);
      break;
    }
    case PROP_RETURN_VALUE:
      /* not settable */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
sso_ui_dialog_dispose (GObject *object)
{
  SSOUIDialog *self = SSO_UI_DIALOG (object);

  if (self->priv->params) {
    gsignond_signonui_data_unref (self->priv->params);
    self->priv->params = NULL;
  }
#if 0
  g_free (self->priv->test_reply);
  self->priv->test_reply = NULL;
#endif
  if (self->priv->oauth_response) {
    g_free (self->priv->oauth_response);
    self->priv->oauth_response = NULL;
  }

  if (self->priv->dialog) {
    evas_object_del (self->priv->dialog);
    self->priv->dialog = NULL;
  }
#ifndef USE_WEBKIT
  stop_http_server(self);
  stop_browser_exe(self);
#endif // USE_WEBKIT

  G_OBJECT_CLASS (sso_ui_dialog_parent_class)->dispose (object);
}

static void
sso_ui_dialog_class_init (SSOUIDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SSOUIDialogPrivate));

  object_class->get_property = sso_ui_dialog_get_property;
  object_class->set_property = sso_ui_dialog_set_property;
  object_class->dispose = sso_ui_dialog_dispose;

  g_object_class_install_property (object_class, PROP_INVOCATION,
      g_param_spec_object ("invocation", "Invocation",
                           "The GDBusMethodInvocation object for this authentication",
                           G_TYPE_DBUS_METHOD_INVOCATION,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_PARAMETERS,
      g_param_spec_boxed ("parameters", "Parameters",
                            "The GVariant a{sv} that contains the parameters for this dialog",
                            GSIGNOND_TYPE_SIGNONUI_DATA, 
                            G_PARAM_WRITABLE));

  g_object_class_install_property (object_class, PROP_RETURN_VALUE,
      g_param_spec_boxed ("return-value", "Return value",
                            "The return value for this dialog. Only defined after closed signal",
                            GSIGNOND_TYPE_SIGNONUI_DATA,
                            G_PARAM_READABLE));

  signals[CLOSED_SIGNAL] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (SSOUIDialogClass, closed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

}

static void
sso_ui_dialog_init (SSOUIDialog *self)
{
  SSOUIDialogPrivate *priv = UI_DIALOG_PRIVATE (self);

  priv->params = NULL;
  priv->error_code = SIGNONUI_ERROR_NONE;
  priv->old_password = NULL;
  priv->oauth_final_url = NULL;
  priv->oauth_response = NULL;
  
  priv->dialog  = NULL;
  priv->message = NULL;
  priv->username_entry = NULL;
  priv->password_entry = NULL;
  priv->password_confirm1_entry = NULL;
  priv->password_confirm2_entry = NULL;
  priv->remember_check = NULL;
  priv->captcha_image = NULL;
  priv->captcha_entry = NULL;
#ifdef USE_WEBKIT
  priv->oauth_web = NULL;
#else
  priv->browser_exe = NULL;
  priv->browser_event_handler = NULL;
  priv->http_server = NULL;
#endif // USE_WEBKIT

  self->priv = priv;
}

static void
close_dialog (SSOUIDialog *self)
{
  g_debug ("Dialog %s closed", gsignond_signonui_data_get_request_id (
                self->priv->params));

  if (self->priv->dialog) {
    evas_object_hide (self->priv->dialog);
    self->priv->dialog = NULL;
  }

  g_signal_emit (self, signals[CLOSED_SIGNAL], 0);
}

static void
on_cancel_or_close_clicked (void *data, Evas_Object *obj, void *event_info)
{
  SSOUIDialog *self = SSO_UI_DIALOG (data);

  self->priv->error_code = SIGNONUI_ERROR_CANCELED;
  close_dialog (self);
}

static void
on_ok_clicked (void *data, Evas_Object *obj, void *event_info)
{
  SSOUIDialog *self = SSO_UI_DIALOG (data);
  SSOUIDialogPrivate *priv = self->priv;
  gboolean query_confirm = FALSE;

  gsignond_signonui_data_get_confirm (priv->params, &query_confirm);

  if (query_confirm) {
    g_assert (priv->old_password);

    if (g_strcmp0 (priv->old_password,
                   elm_entry_entry_get (priv->password_entry)) != 0) {
      g_debug ("Old password does not match, not accepting.");
      return;
    }
    if (g_strcmp0 (elm_entry_entry_get (priv->password_confirm1_entry),
                   elm_entry_entry_get (priv->password_confirm2_entry)) != 0) {
      g_debug ("New passwords do not match, not accepting.");
      return;
    }
  }

  priv->error_code = SIGNONUI_ERROR_NONE;
  close_dialog (self);
}

static void
on_forgot_clicked (void *data, Evas_Object *obj, void *event_info)
{
  SSOUIDialog *self = SSO_UI_DIALOG (data);
  GError *error = NULL;

  const gchar *forgot_url = 
        gsignond_signonui_data_get_forgot_password_url (self->priv->params);

  if (!forgot_url) {
    g_warning ("'forgot password' URL is not set.");
    return;
  }

  if (!g_app_info_launch_default_for_uri (forgot_url,
                                          NULL,
                                          &error)) {
    g_warning ("Failed to launch default handler for '%s': %s.",
               forgot_url, error->message);
    g_clear_error (&error);
    return;
  }

  g_debug ("Launched default handler for 'forgot password' URL %s",
           forgot_url);
  self->priv->error_code = SIGNONUI_ERROR_FORGOT_PASSWORD;
  close_dialog (self);
}

#ifdef USE_WEBKIT
static void
on_web_url_change (void *data, Evas_Object *obj, void *event_info)
{
  SSOUIDialog *self = data;
  const char *uri = event_info;

  if (!self->priv->oauth_final_url ||
      !g_str_has_prefix (uri, self->priv->oauth_final_url))
    return;

   g_debug ("FOUND URL : %s", uri);
   self->priv->oauth_response = g_strdup (uri);

  self->priv->error_code = SIGNONUI_ERROR_NONE;
  close_dialog (self);
}

static void
on_web_url_load_error (void *data, Evas_Object *obj, void *event_info)
{
    const Ewk_Error* error = (const Ewk_Error*)event_info;

    g_debug ("%s, error %s", __FUNCTION__, ewk_error_description_get(error));
}

static void
on_web_url_load_finished (void *data, Evas_Object *obj, void *event_info)
{
    g_debug("%s", __FUNCTION__);
}
#else

static void
http_server_cb(SoupServer* server, SoupMessage* message, const char* path,
                GHashTable* query, SoupClientContext* client,
                gpointer userdata) {
  SSOUIDialog *self = SSO_UI_DIALOG(userdata);
  SoupURI* uri = soup_message_get_uri(message);

  if (strcmp(path, "/") != 0)
    return;

  const char *response = "\
    <html><head></head>\
    <body><script>\
      window.onload = function() {\
        console.log('Window loaded ....');\
        window.close();\
      };\
    </script>\
    <p>Authentication successfull. You can close the window to continue.</body>\
    </html>";

  soup_message_set_status(message, SOUP_STATUS_OK);
  soup_message_set_response(message, "text/html", SOUP_MEMORY_COPY,
      response, strlen(response));

  stop_browser_exe(self);
  stop_http_server(self);

// FIXME: if we pick our own http port number we should revert it
//        from the responce uri. but we not yet support this as changes
//        needed from oauth plugin to support this custom http port number.
//  SoupURI *redirect_uri = soup_uri_new(self->priv->oauth_final_url);
//  if (soup_uri_get_port (uri) != soup_uri_get_port (redirect_uri))
//    soup_uri_set_port (uri, soup_uri_get_port (redirect_uri));
//  soup_uri_free (redirect_uri);
  self->priv->oauth_response = g_strdup (soup_uri_to_string(uri, FALSE));
  g_debug ("Oauth Responce : %s", self->priv->oauth_response);
  self->priv->error_code = SIGNONUI_ERROR_NONE;
  close_dialog (self);
}

static gboolean 
start_http_server(SSOUIDialog *self) {
  if (self->priv->http_server) return TRUE;

  SoupURI *uri = soup_uri_new(self->priv->oauth_final_url);
  if (!uri) {
    g_warning("NULL redirect url : %s", self->priv->oauth_final_url);
    return FALSE;
  }
  
  if (!soup_uri_get_scheme(uri) || !soup_uri_get_host(uri)) {
    g_warning("Not a valid redirect url : %s", self->priv->oauth_final_url);
    soup_uri_free(uri);
    return FALSE;
  }

  if (strcmp(soup_uri_get_scheme(uri), "http") != 0 ||
      (strcmp(soup_uri_get_host(uri), "localhost") != 0 &&
       strcmp(soup_uri_get_host(uri), "127.0.0.1") != 0)) {
    g_warning("Not a localhost redirect uri '%s', so not starting http server",
        self->priv->oauth_final_url);
    soup_uri_free(uri);
    return FALSE;
  }

  /* if no port specified in redirect_url use the default one
   * and update the oauth_url with that port number
   */
  if (soup_uri_uses_default_port(uri)) {
    soup_uri_set_port(uri, http_port);
    SoupURI *oauth_uri = soup_uri_new(
        gsignond_signonui_data_get_open_url(self->priv->params));
    GHashTable *query = soup_form_decode_urlencoded(
      soup_uri_get_query(oauth_uri));
    g_hash_table_insert(query, g_strdup("redirect_uri"),
        soup_uri_to_string(uri, FALSE));

    soup_uri_set_query_from_form(oauth_uri, query);

    g_hash_table_destroy(query);

    gsignond_signonui_data_set_open_url(self->priv->params,
        soup_uri_to_string(oauth_uri, FALSE));
    soup_uri_free(oauth_uri);
  }

  guint port = soup_uri_get_port(uri);
  soup_uri_free(uri);
  // start http server to listen for authentication results
  SoupServer *http_server = soup_server_new(SOUP_SERVER_PORT, port, NULL);
  if (!http_server) {
    g_warning("Failed to start http server ");
    return FALSE;
  }
  g_debug("Started http server at port '%u'",
      soup_server_get_port(http_server));

  soup_server_add_handler(http_server, "/", http_server_cb, self, NULL);
  soup_server_run_async(http_server);
  self->priv->http_server = http_server;

  return TRUE;
}

static void
stop_http_server(SSOUIDialog *self) {
  if (!self || !self->priv->http_server) return ;
  soup_server_quit(self->priv->http_server);
  soup_server_disconnect(self->priv->http_server);
  g_clear_object(&self->priv->http_server);
}

static Eina_Bool
handle_browser_exe_event(void *data, int type, void *event) {
  if (type != ECORE_EXE_EVENT_DEL) return 1;
  SSOUIDialog *self = SSO_UI_DIALOG(data);

  g_debug("Browser process dead");
  if (self->priv->browser_exe) {
    ecore_event_handler_del(self->priv->browser_event_handler);
    self->priv->browser_exe = NULL;
  }

  self->priv->error_code = SIGNONUI_ERROR_CANCELED;
  close_dialog(self);

  return 0;
}

static gboolean
start_browser_exe(SSOUIDialog *self, const gchar *url) {
  if (self->priv->browser_exe) return TRUE;

  pid_t browser_exe_id;
  char cmd[1024];
  snprintf(cmd, 1023, "%s '%s'", browser_cmd, url);
  g_debug("Launching browser : %s", cmd);
  self->priv->browser_exe = ecore_exe_pipe_run(
      cmd, ECORE_EXE_TERM_WITH_PARENT, NULL);
  if (!self->priv->browser_exe) {
   g_warning("Failed to start browser session");
   return FALSE;
  }
  browser_exe_id = ecore_exe_pid_get(self->priv->browser_exe);

  g_debug("Brower process started with pid '%u'", browser_exe_id);

  snprintf(cmd, 1023, "pgrep -P %u > /home/avalluri/pids", browser_exe_id);
  if (system(cmd) == -1)
    g_debug("Failed to pgrep");;
  
  self->priv->browser_event_handler = ecore_event_handler_add(
      ECORE_EXE_EVENT_DEL, handle_browser_exe_event, self);

  return TRUE;
}

static void
stop_browser_exe(SSOUIDialog *self) {
  if (self->priv->browser_exe) {
    ecore_event_handler_del(self->priv->browser_event_handler);
    ecore_exe_kill(self->priv->browser_exe); 
    ecore_exe_free(self->priv->browser_exe);
    self->priv->browser_exe = NULL;
  }
}

#endif // USE_WEBKIT

static Evas_Object*
add_entry (Evas_Object *window, Evas_Object *container, const gchar *label_text)
{
  Evas_Object *frame = NULL;
  Evas_Object *entry = NULL;

  if (label_text) {
    frame = elm_frame_add(window);

    elm_object_text_set(frame, label_text);
    evas_object_size_hint_weight_set(frame, 0.0, 0.0);
    evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(container, frame);
    evas_object_show(frame);
  }

  entry = elm_entry_add (window);
  elm_entry_single_line_set (entry, EINA_TRUE);
  elm_entry_scrollable_set (entry, EINA_TRUE);
  evas_object_size_hint_min_set (entry, 150, 80);
  evas_object_size_hint_align_set (entry,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0.0);
  frame ? elm_object_content_set (frame, entry) 
        : elm_box_pack_end (container, entry);

  return entry;
}

static gboolean
build_dialog (SSOUIDialog *self)
{
  SSOUIDialogPrivate *priv = self->priv;

  const gchar *str = NULL;
#ifndef USE_WEBKIT
  if (gsignond_signonui_data_get_open_url (priv->params) != NULL) {
    priv->oauth_final_url = gsignond_signonui_data_get_final_url (priv->params);
    if (!start_http_server(self)) {
      close_dialog(self);
      return FALSE;
    }
    start_browser_exe(self, gsignond_signonui_data_get_open_url (priv->params));
    return TRUE;
  }
#endif // USE_WEBKIT
  Evas_Object *bg, *box, *frame, *content_box;
  Evas_Object *button_frame, *pad_frame, *button_box;
  Evas_Object *cancel_button, *ok_button;

#if 0
  priv->test_reply = gsignond_signonui_data_get_test_reply (priv->params);
#endif
  /* main window */
  priv->dialog = elm_win_add (NULL, "dialog", ELM_WIN_BASIC);
  str = gsignond_signonui_data_get_title(priv->params);
  elm_win_title_set (priv->dialog, str ? str : "Single Sign-On");
  elm_win_center (priv->dialog, EINA_TRUE, EINA_TRUE);
  evas_object_smart_callback_add (priv->dialog, "delete,request",
                                  on_cancel_or_close_clicked, self);

  /* window background */
  bg = elm_bg_add (priv->dialog);
  evas_object_size_hint_weight_set (bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show (bg);
  elm_win_resize_object_add (priv->dialog, bg);

  box = elm_box_add (priv->dialog);
  evas_object_size_hint_min_set (box, 200, 200);
  evas_object_size_hint_weight_set (box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show (box);
  elm_win_resize_object_add (priv->dialog, box);

  frame = elm_frame_add (priv->dialog);
  elm_object_style_set (frame, "pad_small");
  evas_object_size_hint_weight_set (frame,
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (frame,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_show (frame);
  elm_box_pack_start (box, frame);

  content_box = elm_box_add (priv->dialog);
  elm_box_padding_set (content_box, 0, 3);
  evas_object_size_hint_weight_set (content_box, 
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set (content_box, 0.0, 0.0);
  evas_object_show (content_box);
  elm_object_part_content_set (frame, NULL, content_box);

  /* Web Dialog for Outh */
  if ((str = gsignond_signonui_data_get_open_url (priv->params))) {
#ifdef USE_WEBKIT
    Evas *canvas = NULL;
    priv->oauth_final_url = gsignond_signonui_data_get_final_url (priv->params);
    g_debug ("Loading url : %s", str);

    canvas = evas_object_evas_get (priv->dialog);
    priv->oauth_web = ewk_view_add(canvas);

    if (!ewk_view_url_set(priv->oauth_web, str))
      g_warning ("Failed to set URI '%s'", str);

    evas_object_size_hint_min_set (priv->oauth_web, 400, 300);
    evas_object_size_hint_weight_set(priv->oauth_web,
                                     EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (priv->oauth_web,
                                     EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (priv->oauth_web, "url,changed",
                                  on_web_url_change, self);
    evas_object_smart_callback_add (priv->oauth_web, "load,error",
                                  on_web_url_load_error, self);
    evas_object_smart_callback_add (priv->oauth_web, "load,finished",
                                  on_web_url_load_finished, self);
    elm_box_pack_end (content_box, priv->oauth_web);
    evas_object_show(priv->oauth_web);
#endif // USE_WEBKIT
  }
  else {
    gboolean query_username = FALSE;
    gboolean remember = FALSE;
    gboolean confirm = FALSE;
    /* credentials */
    if ((str = gsignond_signonui_data_get_caption (priv->params))) {
      Evas_Object *caption_label = elm_label_add (priv->dialog);
      elm_object_text_set (caption_label, str);
      evas_object_size_hint_align_set (caption_label,
                                   0.0, 0.0);
      evas_object_show (caption_label);
      elm_box_pack_end (content_box, caption_label);
    }

    priv->username_entry = add_entry (priv->dialog, content_box, "Username:");
    gsignond_signonui_data_get_query_username (priv->params, &query_username);
    str = gsignond_signonui_data_get_username (priv->params);
    elm_entry_entry_set (priv->username_entry, str ? str : "");
    g_debug ("Settin username entry to editable %d",
        query_username || !str ? EINA_TRUE : EINA_FALSE);
    elm_entry_editable_set (priv->username_entry,
        query_username || !str ? EINA_TRUE : EINA_FALSE);

    priv->password_entry = add_entry (priv->dialog, content_box, "Password:");
    elm_entry_password_set (priv->password_entry, EINA_TRUE);
    if (gsignond_signonui_data_get_confirm (priv->params, &confirm) && confirm) {
      priv->old_password = gsignond_signonui_data_get_password (priv->params);
      elm_entry_entry_set (priv->password_entry,
          priv->old_password ? priv->old_password : "");
      elm_entry_editable_set (priv->password_entry, EINA_FALSE);
        
      priv->password_confirm1_entry = add_entry (priv->dialog,
          content_box, "New Password:");
      elm_entry_password_set (priv->password_confirm1_entry, EINA_TRUE);

      priv->password_confirm2_entry = add_entry (priv->dialog, content_box,
          "Confirm New Password:");
      elm_entry_password_set (priv->password_confirm2_entry, EINA_TRUE);
    }
    else 
      elm_entry_editable_set (priv->password_entry, EINA_TRUE);

    /* remember password */
    priv->remember_check = elm_check_add (priv->dialog);
    elm_object_text_set (priv->remember_check, "Remember password");
    evas_object_show (priv->remember_check);
    gsignond_signonui_data_get_remember_password (priv->params, &remember);
    elm_check_state_set (priv->remember_check, remember);
    evas_object_size_hint_align_set (priv->remember_check,
                                   1.0, EVAS_HINT_FILL);
    elm_box_pack_end (content_box, priv->remember_check);

    /* Forgot password link */
    if ((str =gsignond_signonui_data_get_forgot_password_url (priv->params))) {
      Evas_Object *forgot_button = elm_button_add (priv->dialog);
      elm_object_text_set (forgot_button, "Forgot password");
      evas_object_smart_callback_add (forgot_button, "clicked",
                                  on_forgot_clicked, self);
      evas_object_size_hint_align_set (forgot_button,
                                   1.0, EVAS_HINT_FILL);
      evas_object_size_hint_weight_set (forgot_button, 0.0, 0.0);
      evas_object_show (forgot_button);
      elm_box_pack_end (content_box, forgot_button);
    }

   /* sigh, Elm_Image can't load urls, and ecore can't use temp files sanely*/
    /* double sigh, ecore_file_download() does not work with file:// ??? */
    if ((str = gsignond_signonui_data_get_captcha_url (priv->params))) { 
      priv->captcha_image = elm_image_add (priv->dialog);
      elm_image_aspect_fixed_set (priv->captcha_image, TRUE);
      evas_object_size_hint_align_set (priv->captcha_image,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
      elm_box_pack_end (content_box, priv->captcha_image);
      priv->captcha_entry = add_entry (priv->dialog, content_box, NULL);

      sso_ui_dialog_refresh_captcha (self, str);
    }

    if ((str = gsignond_signonui_data_get_message (priv->params))) {
      priv->message = elm_label_add (priv->dialog);
      elm_object_text_set (priv->message, str);
      evas_object_size_hint_align_set (priv->message,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
      elm_box_pack_end (content_box, priv->message);
    }

    /* button row */

    button_frame = elm_frame_add (priv->dialog);
    elm_object_style_set (button_frame, "outdent_bottom");
    evas_object_size_hint_weight_set (button_frame, 0.0, 0.0);
    evas_object_size_hint_align_set (button_frame,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show (button_frame);
    elm_box_pack_end (box, button_frame);

    pad_frame = elm_frame_add (priv->dialog);
    elm_object_style_set (pad_frame, "pad_medium");
    evas_object_show (pad_frame);
    elm_object_part_content_set (button_frame, NULL, pad_frame);

    button_box = elm_box_add (priv->dialog);
    elm_box_horizontal_set (button_box, 1);
    elm_box_homogeneous_set (button_box, 1);
    evas_object_show (button_box);
    elm_object_part_content_set (pad_frame, NULL, button_box);


    /* Cancel button */
    cancel_button = elm_button_add (priv->dialog);
    elm_object_text_set (cancel_button, "Cancel");
    evas_object_size_hint_weight_set (cancel_button,
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (cancel_button,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (cancel_button, "clicked",
                                 on_cancel_or_close_clicked, self);
    evas_object_show (cancel_button);
    elm_box_pack_end (button_box, cancel_button);

    /* OK button */
    ok_button = elm_button_add (priv->dialog);
    elm_object_text_set (ok_button, "OK");
    evas_object_size_hint_weight_set (ok_button,
                                    EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set (ok_button,
                                   EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_smart_callback_add (ok_button, "clicked", on_ok_clicked, self);
    evas_object_show (ok_button);
    elm_box_pack_end (button_box, ok_button);
  }

  return TRUE;
}

static gboolean
validate_params(GSignondSignonuiData *params) {
  gboolean query_username = FALSE;
  gboolean query_password = FALSE;
  gboolean query_confirm = FALSE;
  const gchar *request_id = gsignond_signonui_data_get_request_id (params);
  gboolean query_oauth = 
    gsignond_signonui_data_get_open_url(params) != NULL;
  gsignond_signonui_data_get_query_username (params, &query_username);
  gsignond_signonui_data_get_query_password (params, &query_password);
  gsignond_signonui_data_get_confirm (params, &query_confirm);

  if(!request_id ||
     !(query_oauth || query_username || query_password || query_confirm))
    return FALSE;
  if (query_oauth)
    return !(query_username || query_password || query_confirm) &&
             (gsignond_signonui_data_get_open_url(params) &&
              gsignond_signonui_data_get_final_url(params));
  if (query_confirm)
    return gsignond_signonui_data_get_password(params) != NULL;

  return TRUE;
}

void
sso_ui_dialog_set_parameters (SSOUIDialog *self,
                              GSignondSignonuiData *parameters)
{
  SSOUIDialogPrivate *priv = self->priv;

  if (!validate_params (parameters)) { 
    priv->error_code = SIGNONUI_ERROR_BAD_PARAMETERS;
    return;
  }

  priv->params = gsignond_signonui_data_ref (parameters);
  build_dialog (self);
}

gboolean
sso_ui_dialog_refresh_captcha (SSOUIDialog *self, const gchar *url)
{
  g_return_val_if_fail (self && SSO_IS_UI_DIALOG (self), FALSE);
  g_return_val_if_fail (url, FALSE);

  gchar *filename = g_filename_from_uri (url, NULL, NULL);

  g_debug ("Using captcha %s", filename);
  if (!filename) return FALSE;
  elm_image_file_set (self->priv->captcha_image, filename, NULL);
  g_free (filename);

  return TRUE;
}

GDBusMethodInvocation*
sso_ui_dialog_get_invocation (SSOUIDialog *self)
{
  return self->priv->invocation;
}

const gchar*
sso_ui_dialog_get_request_id (SSOUIDialog *self)
{
  return gsignond_signonui_data_get_request_id (self->priv->params);
}

GSignondSignonuiData*
sso_ui_dialog_get_return_value (SSOUIDialog *self)
{
  SSOUIDialogPrivate *priv = self->priv;
  GSignondSignonuiData *reply = gsignond_signonui_data_new ();

  gsignond_signonui_data_set_query_error (reply, priv->error_code);

  if (priv->error_code == SIGNONUI_ERROR_NONE) {
    gboolean query_oauth = gsignond_signonui_data_get_open_url (priv->params) != NULL;

    if (query_oauth) {
      gsignond_signonui_data_set_url_response (reply, priv->oauth_response);
    }
    else {
      gboolean query_username = FALSE;
      gboolean query_confirm = FALSE;
      gsignond_signonui_data_get_query_username (priv->params, &query_username);
      gsignond_signonui_data_get_confirm (priv->params, &query_confirm);

      gsignond_signonui_data_set_password (reply,
           elm_entry_entry_get (query_confirm ? 
               priv->password_confirm1_entry : priv->password_entry)); 
      if (priv->remember_check) {
        gsignond_signonui_data_set_remember_password (reply,
                               elm_check_state_get (priv->remember_check));
      }

      if (priv->captcha_entry) {
        gsignond_signonui_data_set_captcha_response (reply,
            elm_entry_entry_get (priv->captcha_entry));
      }
      if (query_username) {
        gsignond_signonui_data_set_username (reply,
               elm_entry_entry_get (priv->username_entry));
      }
    }
  }

  return reply;
}
#if 0
static void
sso_ui_dialog_handle_test_reply (SSOUIDialog *self)
{
  char **iter;
  char **pairs = g_strsplit (self->priv->test_reply, ",", 0);
  for (iter = pairs; *iter; iter++) {
    char **pair = g_strsplit (*iter, ":", 2);
    if (g_strv_length (pair) == 2) {
      if (g_strcmp0 (pair[0], SSO_UI_KEY_CAPTCHA_RESPONSE) == 0) {
        if (evas_object_visible_get (self->priv->captcha_entry))
          elm_entry_entry_set (self->priv->captcha_entry, pair[1]);
      } else if (g_strcmp0 (pair[0], SSO_UI_KEY_PASSWORD) == 0) {
        if (evas_object_visible_get (self->priv->password_entry))
          elm_entry_entry_set (self->priv->password_entry, pair[1]);
        if (evas_object_visible_get (self->priv->password_confirm1_entry))
          elm_entry_entry_set (self->priv->password_confirm1_entry, pair[1]);
      } else if (g_strcmp0 (pair[0], SSO_UI_KEY_QUERY_ERROR_CODE) == 0) {
        self->priv->error_code = atoi(pair[1]);
      } else if (g_strcmp0 (pair[0], SSO_UI_KEY_REMEMBER_PASSWORD) == 0) {
        if (evas_object_visible_get (self->priv->remember_check))
          elm_check_state_set (self->priv->remember_check, g_strcmp0 (pair[1], "True") == 0);
      } else if (g_strcmp0 (pair[0], SSO_UI_KEY_URL_RESPONSE) == 0) {
        if (evas_object_visible_get (self->priv->oauth_web))
          self->priv->oauth_response = g_strdup (pair[1]);
      } else if (g_strcmp0 (pair[0], SSO_UI_KEY_USERNAME) == 0) {
        if (evas_object_visible_get (self->priv->username_entry))
          elm_entry_entry_set (self->priv->username_entry, pair[1]);
      }
    }
    g_strfreev (pair);
  }
  g_strfreev (pairs);
}
#endif
gboolean
sso_ui_dialog_show (SSOUIDialog *self)
{
  g_return_val_if_fail(SSO_IS_UI_DIALOG(self), FALSE);

  SSOUIDialogPrivate *priv = self->priv;
  if (priv->error_code != SIGNONUI_ERROR_NONE) return FALSE;

  if (gsignond_signonui_data_get_open_url(self->priv->params)) {
#ifdef USE_WEBKIT
    if (!priv->oauth_web) {
#else
    if (!(priv->browser_exe && priv->http_server)) {
#endif // USE_WEBKIT
      priv->error_code = SIGNONUI_ERROR_NOT_AVAILABLE;
      return FALSE;
    }
  } else {
    if (!priv->dialog)
      return FALSE;
    evas_object_show (priv->dialog);
  }

  return TRUE;
}

void
sso_ui_dialog_close (SSOUIDialog *self)
{
  /* TODO : is this really considered cancel ? */
  self->priv->error_code = SIGNONUI_ERROR_CANCELED;
  close_dialog (self);
}

SSOUIDialog*
sso_ui_dialog_new (GSignondSignonuiData *parameters,
                   GDBusMethodInvocation *invocation)
{
  return g_object_new (SSO_TYPE_UI_DIALOG,
                       "parameters", parameters,
                       "invocation", invocation,
                       NULL);
}

