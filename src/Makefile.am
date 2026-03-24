AM_CPPFLAGS = \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	$(LIBGPIOD_CFLAGS)

bin_PROGRAMS = \
	mausberry-switch

mausberry_switch_LDADD = \
	$(LIBGPIOD_LIBS)

mausberry_switch_SOURCES = \
	main.c
