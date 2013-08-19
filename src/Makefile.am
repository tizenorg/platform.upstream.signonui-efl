libexec_PROGRAMS = signon-ui

BUILT_SOURCES = \
    sso-ui-dbus-glue.c \
    sso-ui-dbus-glue.h \
    sso-ui-dialog-dbus-glue.c \
    sso-ui-dialog-dbus-glue.h

sso-ui-dbus-glue.c sso-ui-dbus-glue.h: ../data/singlesignonui.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix com.google.code.AccountsSSO.gSingleSignOn \
	--generate-c-code sso-ui-dbus-glue \
    --c-namespace SSO_Dbus \
	$<

sso-ui-dialog-dbus-glue.c sso-ui-dialog-dbus-glue.h: ../data/singlesignonui-dialog.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix com.google.code.AccountsSSO.gSingleSignOn \
	--generate-c-code sso-ui-dialog-dbus-glue \
    --c-namespace SSO_Dbus \
	$<

signon_ui_SOURCES = \
	$(BUILT_SOURCES) \
	sso-ui-dialog.c \
	sso-ui-dialog.h \
	sso-ui.c

signon_ui_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(EFL_CFLAGS) \
    $(GSIGNOND_CFLAGS) \
    -Wall -Werror \
	-I$(top_srcdir)

signon_ui_LDADD = \
	$(GLIB_LIBS) \
	$(EFL_LIBS) \
    $(GSIGNOND_LIBS)

CLEANFILES = \
	$(BUILT_SOURCES) \
	*~
