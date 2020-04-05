chessdconf = $(top_srcdir)/etc/chessd.conf

EXTRA_DIST += $(chessdconf)
install-chessdconf:
	test -d "$(sysconfdir)" || $(mkdir_p) "$(sysconfdir)"
	@if ! [ -f "$(sysconfdir)/chessd.conf" ]; then \
		echo "$(INSTALL) -m 0600 '$(chessdconf)' \"$(sysconfdir)\"" ;\
		$(INSTALL) -m 0600 '$(chessdconf)' "$(sysconfdir)" ;\
	fi
install-data-local: install-chessdconf
