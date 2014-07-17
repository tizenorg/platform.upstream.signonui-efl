libexec_PROGRAMS = signonui-efl

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

signonui_efl_SOURCES = \
	$(BUILT_SOURCES) \
	sso-ui-dialog.c \
	sso-ui-dialog.h \
	sso-ui.c

signonui_efl_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(EFL_CFLAGS) \
    $(GSIGNOND_CFLAGS) \
    -Wall -Werror \
	-I$(top_srcdir)

signonui_efl_LDADD = \
	$(GLIB_LIBS) \
	$(EFL_LIBS) \
  $(GSIGNOND_LIBS)

if WITH_WEBKIT
signonui_efl_CFLAGS += $(EWEBKIT_CFLAGS)
signonui_efl_LDADD += $(EWEBKIT_LIBS) 
else
signonui_efl_CFLAGS += $(LIBSOUP_CFLAGS)
signonui_efl_LDADD += $(LIBSOUP_LIBS) 
endif

CLEANFILES = \
	$(BUILT_SOURCES) \
	*~
