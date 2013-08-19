
noinst_DATA = singlesignonui.xml singlesignonui-dialog.xml

servicedir = $(datadir)/dbus-1/services
service_in_files = com.google.code.AccountsSSO.gSingleSignOnUI.service.in
service_DATA = $(service_in_files:.service.in=.service)

com.google.code.AccountsSSO.gSingleSignOnUI.service: com.google.code.AccountsSSO.gSingleSignOnUI.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(noinst_DATA) \
             $(service_in_files)

CLEANFILES = $(service_DATA)
