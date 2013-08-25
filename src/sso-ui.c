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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <Ecore.h>
#include <Elementary.h>
#include <EWebKit2.h>
#include <gsignond/gsignond-signonui-data.h>

#include "sso-ui-dbus-glue.h"
#include "sso-ui-dialog-dbus-glue.h"
#include "sso-ui-dialog.h"

#ifndef DAEMON_TIMEOUT
#   define DAEMON_TIMEOUT (30) // seconds
#endif

#define SSO_UI_NAME "com.google.code.AccountsSSO.gSingleSignOn.UI"

/*
   FIXME : below defs should be remove once we revert 
   change in gsignond
*/
#ifndef gsignond_signonui_data_new_from_variant
#   define gsignond_signonui_data_new_from_variant gsignond_dictionary_new_from_variant
#endif
#ifndef gsignond_signonui_data_to_variant
#   define gsignond_signonui_data_to_variant gsignond_dictionary_to_variant
#endif
#ifndef gsignond_signonui_data_unref
#   define gsignond_signonui_data_unref gsignond_dictionary_unref
#endif

GDBusServer *bus_server = NULL; /* p2p server */
GHashTable *dialogs;
gchar *socket_file_path = NULL; /* p2p file system socket path */

#ifdef ENABLE_TIMEOUT
Ecore_Timer *exit_timer_id = 0;
double timeout;

static Eina_Bool 
exit_sso_ui (gpointer data)
{
  g_debug ("Exiting since no calls in a while.");

  elm_exit ();

  if (exit_timer_id) {
      ecore_timer_del (exit_timer_id);
      exit_timer_id = 0;
  }

  return ECORE_CALLBACK_CANCEL;
}
#endif

static void
return_value (SSOUIDialog *dialog, SSODbusUIDialog *dbus_ui_dialog)
{
  GSignondSignonuiData *result = NULL;
  GDBusMethodInvocation *invocation = NULL;
  GVariant *var = NULL;

  result = sso_ui_dialog_get_return_value (dialog);
  invocation = sso_ui_dialog_get_invocation (dialog);
  var = gsignond_signonui_data_to_variant (result);
  /* consumes floating result GVariant */
  sso_dbus_uidialog_complete_query_dialog (dbus_ui_dialog,invocation, var);
  gsignond_signonui_data_unref (result);
}

static void
on_dialog_close (SSOUIDialog *dialog,
                 gpointer     userdata)
{
  return_value (dialog, SSO_DBUS_UIDIALOG(userdata));
  g_hash_table_remove (dialogs, sso_ui_dialog_get_request_id (dialog));

#ifdef ENABLE_TIMEOUT
  if (g_hash_table_size (dialogs) == 0 && exit_timer_id) {
    ecore_timer_thaw (exit_timer_id);
  }
#endif
}

static gboolean
on_query_dialog (SSODbusUIDialog       *ui,
                 GDBusMethodInvocation *invocation,
                 GVariant              *var,
                 gpointer               user_data)
{
  SSOUIDialog* dialog;
  g_debug ("method 'QueryDialog' called");

  GSignondSignonuiData *params = gsignond_signonui_data_new_from_variant (var);
  dialog = sso_ui_dialog_new (params, invocation);
  gsignond_signonui_data_unref (params);
  g_signal_connect (dialog, "closed",
                    G_CALLBACK (on_dialog_close), user_data);


  if (!sso_ui_dialog_show (dialog)) {
    /* there's an error, return it */
    return_value (dialog, ui);
  } else {
    g_object_set_data (G_OBJECT (user_data), "dialog", dialog);
    g_hash_table_insert (dialogs,
                         (gpointer)sso_ui_dialog_get_request_id (dialog),
                         (gpointer)dialog);
#ifdef ENABLE_TIMEOUT
    if (exit_timer_id) ecore_timer_freeze (exit_timer_id);
#endif
  }

  return TRUE;
}

static gboolean
on_refresh_dialog (SSODbusUIDialog       *ui,
                   GDBusMethodInvocation *invocation,
                   GVariant              *var,
                   gpointer               user_data)
{
  SSOUIDialog *old_dialog;
  GSignondSignonuiData *data = gsignond_signonui_data_new_from_variant (var);
  const gchar *url = NULL;
  g_debug ("method 'RefreshDialog' called");

  old_dialog = g_hash_table_lookup (dialogs, 
    gsignond_signonui_data_get_request_id (data));
  if (old_dialog && (url = gsignond_signonui_data_get_captcha_url (data)))
    sso_ui_dialog_refresh_captcha (old_dialog, url);

  gsignond_signonui_data_unref (data);

  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static gboolean
on_cancel_ui_request (SSODbusUIDialog       *ui,
                      GDBusMethodInvocation *invocation,
                      char                  *request_id,
                      gpointer               user_data)
{
  g_debug ("method 'CancelUiRequest' called");

  if (request_id) {
    SSOUIDialog *dialog;
    dialog = g_hash_table_lookup (dialogs, request_id);
    if (dialog)
      sso_ui_dialog_close (dialog);
  }

  g_dbus_method_invocation_return_value (invocation, NULL);
  return TRUE;
}

static void 
_on_connection_closed (GDBusConnection *connection,
                       gboolean         remote_peer_vanished,
                       GError          *error,
                       gpointer         user_data)
{
    g_debug ("Client Dis-Connected ....");

    g_signal_handlers_disconnect_by_func (connection, _on_connection_closed, user_data);

    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON(user_data));

    /* close active dialog if any */
    gpointer *dialog = g_object_get_data (G_OBJECT (user_data), "dialog");
    if (dialog && SSO_IS_UI_DIALOG (dialog)) {
        g_object_unref (G_OBJECT (dialog));
    }

    g_object_unref (G_OBJECT(user_data));
}

static gboolean
on_client_connection (GDBusServer *dbus_server,
                      GDBusConnection *connection,
                      gpointer userdata)
{
  SSODbusUIDialog *dbus_dialog = sso_dbus_uidialog_skeleton_new ();

  g_debug ("Client Connected ....");

  g_signal_connect (dbus_dialog, "handle-query-dialog",
                    G_CALLBACK (on_query_dialog), dbus_dialog);
  g_signal_connect (dbus_dialog, "handle-refresh-dialog",
                    G_CALLBACK (on_refresh_dialog), dbus_dialog);
  g_signal_connect (dbus_dialog, "handle-cancel-ui-request",
                    G_CALLBACK (on_cancel_ui_request), dbus_dialog);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (dbus_dialog),
                                         connection,
                                         "/Dialog",
                                         NULL)) {
    g_warning ("Failed to export interface");

    return FALSE;
  }

  g_signal_connect (connection, "closed", G_CALLBACK(_on_connection_closed), dbus_dialog);

  return TRUE;
}

static gboolean
on_get_bus_address (SSODbusUI     *ui,
                    GDBusMethodInvocation *invocation,
                    gpointer       user_data)
{
  if (bus_server) {
    g_debug ("Returning Server address : %s", g_dbus_server_get_client_address (bus_server));
    sso_dbus_ui_complete_get_bus_address (ui, invocation, 
            g_dbus_server_get_client_address (bus_server));
  } else {
    // FIXME: define dbus errors
    //   g_dbus_method_invocation_take_error (invocation,
    //       g_error_new ());
  }

  return TRUE;
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_debug ("D-Bus name acquired");
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_debug ("D-Bus name lost");

  if (bus_server) {
    g_object_unref (bus_server);
    bus_server = NULL;
  }
#ifdef ENABLE_TIMEOUT
  if (exit_timer_id) ecore_timer_del (exit_timer_id);
  exit_timer_id = 0;
#endif
  elm_exit ();
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  gchar *address = NULL;
  gchar *guid = NULL;
  GError *error = NULL;
  SSODbusUI *ui = NULL;
  gchar *base_path = NULL;

  g_debug ("D-Bus bus acquired");
  
  base_path = g_strdup_printf("%s/gsignond/", g_get_user_runtime_dir());
  socket_file_path = tempnam (base_path, "ui-");
  if (g_file_test(socket_file_path, G_FILE_TEST_EXISTS)) {
    g_unlink (socket_file_path);
  }
  else {
    if (g_mkdir_with_parents (base_path, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
      g_warning ("Could not create '%s', error: %s", base_path, strerror(errno));
    }
  }
  g_free (base_path);

  address = g_strdup_printf ("unix:path=%s", socket_file_path);

  guid = g_dbus_generate_guid ();
  bus_server = g_dbus_server_new_sync (address, G_DBUS_SERVER_FLAGS_NONE, guid, NULL, NULL, &error);
  g_free (guid);
  g_free (address);

  if (!bus_server) {
    g_warning ("Could not start dbus server at address '%s' : %s", address, error->message);
    g_error_free (error);

    g_free (socket_file_path);
    socket_file_path = NULL;

    elm_exit();

    return ;
  }

  g_chmod (socket_file_path, S_IRUSR | S_IWUSR);

  g_signal_connect (bus_server, "new-connection", G_CALLBACK (on_client_connection), NULL);

  /* expose interface */

  ui = sso_dbus_ui_skeleton_new ();

  g_signal_connect (ui, "handle-get-bus-address",
                    G_CALLBACK (on_get_bus_address), NULL);

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (ui),
                                         connection,
                                         "/",
                                         &error)) {
    g_warning ("Failed to export interface: %s", error->message);
    g_error_free (error);

    elm_exit();

    return ;
  }

  g_dbus_server_start (bus_server);

  g_debug ("UI Dialog server started at : %s", g_dbus_server_get_client_address (bus_server));
#ifdef ENABLE_TIMEOUT
  if (exit_timer_id) ecore_timer_thaw (exit_timer_id);
#endif
}

static void 
_close_server ()
{
  if (bus_server) {
    g_dbus_server_stop (bus_server);
    g_object_unref (bus_server);
    bus_server = NULL;
  }

  if (socket_file_path) {
    g_unlink (socket_file_path);
    g_free (socket_file_path);
    socket_file_path = NULL;
  }
}

static Eina_Bool
_handle_unix_signal (void *data, int type, void *event)
{
  (void)data;
  (void)event;

  g_debug ("Received signal : %d", type);

  elm_exit();
  return TRUE;
}

EAPI_MAIN int
elm_main (int argc, char **argv)
{
  guint owner_id = 0;
  Ecore_Event_Handler *hup_signal_handler = 0;
  Ecore_Event_Handler *exit_signal_handler = 0;
  Eina_Bool keep_running, help;
  static const Ecore_Getopt optdesc = {
      "gSSO UI daemon",
      NULL,
      "0.0",
      "(C) 2013 Intel Corporation",
      NULL,
      "ui dialog service for gsingle signon daemon",
      0,
      {
          ECORE_GETOPT_STORE_TRUE('k', "keep-running", "Do not timeout the daemon"),
          ECORE_GETOPT_HELP('h', "help"),
          ECORE_GETOPT_SENTINEL
      }
  };
  Ecore_Getopt_Value values[] = {
      ECORE_GETOPT_VALUE_BOOL(keep_running),
      ECORE_GETOPT_VALUE_BOOL(help),
      ECORE_GETOPT_VALUE_NONE
  };

#if !GLIB_CHECK_VERSION (2, 36, 0)
  g_type_init ();
#endif
  ewk_init ();

  if (ecore_getopt_parse (&optdesc, values, argc, argv) < 0) {
      fprintf (stderr, "Argument parsing failed\n");
      return 1;
  }

  if (help) {
      return 0;
  }

#ifdef ENABLE_TIMEOUT
  if (!keep_running) {
    const gchar *env_timeout = g_getenv("GSSO_UI_TIMEOUT");
    if (env_timeout) timeout = atoi (env_timeout);
    if (!timeout) timeout = DAEMON_TIMEOUT;
    exit_timer_id = ecore_timer_add (timeout, exit_sso_ui, NULL);
    /* postpone the timer till DBus setup ready */
    if (exit_timer_id) 
        ecore_timer_freeze (exit_timer_id);
    else {
        fprintf (stderr, "Could not create Ecore Timer, Ignoring daemon timeout");
    }
  }
#endif

  dialogs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                   NULL, g_object_unref);
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             SSO_UI_NAME,
                             G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT | G_BUS_NAME_OWNER_FLAGS_REPLACE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  /* Use the ecore main loop because we intend to use Elm */
  ecore_main_loop_glib_integrate();

  /* install unix signal handler for clean shutdown */
  hup_signal_handler = ecore_event_handler_add (ECORE_EVENT_SIGNAL_HUP, _handle_unix_signal, NULL);
  exit_signal_handler = ecore_event_handler_add (ECORE_EVENT_SIGNAL_EXIT, _handle_unix_signal, NULL);

  elm_run ();

  ecore_event_handler_del (hup_signal_handler);
  ecore_event_handler_del (exit_signal_handler);

  g_bus_unown_name (owner_id);
  _close_server ();
  g_hash_table_unref (dialogs);

  ewk_shutdown ();
  elm_shutdown ();
  g_debug ("Clean shut down");

  return 0;
}
ELM_MAIN()
