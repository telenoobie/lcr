##    Makefile.am -- Process this file with automake to produce Makefile.in

##    This file is part of linux-call-router
##    Copyright (C) 2007 Joerg Habenicht (j.habenicht@gmx.de)
##    Copyright (C) 2008 Peter Schlaile (peter -at- schlaile.de)
##    Copyright (C) 2008-2012 Andreas Eversberg (andreas@eversberg.eu)

##    This program is free software; you can redistribute it and/or
##    modify it under the terms of the GNU General Public License as
##    published by the Free Software Foundation; either
##    version 2 of the License, or (at your option) any later version.

##    This program is distributed in the hope that it will be useful,
##    but WITHOUT ANY WARRANTY; without even the implied warranty of
##    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##    GNU General Public License for more details.

##    You should have received a copy of the GNU General Public License
##    along with this library; see the file COPYING.  If not, write to
##    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
##    Boston, MA 02110-1301, USA.

#INSTALLdir = $(DESTDIR)/usr/local/lcr

pkglogdir=$(localstatedir)/log/@PACKAGE@
pkgsysconfdir=$(sysconfdir)/@PACKAGE@

CONFIGdir=$(pkgsysconfdir)
SHAREdir=$(pkgdatadir)
LOGdir=$(pkglogdir)
EXTENSIONdir=$(localstatedir)/lib/@PACKAGE@/extensions

#CONFIGdir=$(INSTALLdir)
#SHAREdir=$(INSTALLdir)
#LOGdir=$(INSTALLdir)
#EXTENSIONdir=$(INSTALLdir)/extensions

astmoddir = $(libdir)/asterisk/modules

INSTALLATION_DEFINES = \
 -DCONFIG_DATA="\"$(CONFIGdir)\"" \
 -DSHARE_DATA="\"$(SHAREdir)\"" \
 -DLOG_DIR="\"$(LOGdir)\"" \
 -DEXTENSION_DATA="\"$(EXTENSIONdir)\""

SUBDIRS = include

if ENABLE_MISDN

MISDN_INCLUDE = -DWITH_MISDN -DWITH_CRYPT
MISDN_SOURCE = mISDN.cpp fxs.cpp dss1.cpp crypt.cpp
MISDN_LIB = -lmisdn

endif

SUBDIRS =

GSM_INCLUDE =
GSM_SOURCE =
GSM_LIB =

if ENABLE_GSM

#if ENABLE_GSMFR

GSM_INCLUDE += -DWITH_GSMFR

GSM_LIB += libgsmfr/lib/libgsm.a

SUBDIRS += libgsmfr

#endif

if ENABLE_GSMHR

GSM_INCLUDE += -DWITH_GSMHR

GSM_LIB += libgsmhr/libgsmhr.la

SUBDIRS += libgsmhr

endif

if ENABLE_GSMAMR

GSM_INCLUDE += -DWITH_GSMAMR

GSM_LIB += -lopencore-amrnb

endif

GSM_SOURCE += gsm_audio.c gsm.cpp

if ENABLE_GSM_BS

GSM_INCLUDE += -DWITH_GSM_BS

GSM_SOURCE += gsm_bs.cpp

endif

if ENABLE_GSM_MS

GSM_INCLUDE += -DWITH_GSM_MS

GSM_SOURCE += gsm_ms.cpp

endif

endif


if ENABLE_SS5

SS5_INCLUDE = -DWITH_SS5

SS5_SOURCE = ss5.cpp ss5_encode.c ss5_decode.c

endif

SIP_LIB =

if ENABLE_SIP

# FIXME: remove that
#pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = sofia-sip-ua.pc

SIP_INCLUDE = -DWITH_SIP $(SOFIA_CFLAGS)

SIP_SOURCE = sip.cpp

SIP_LIB += $(SOFIA_LIBS)

endif

bin_PROGRAMS = lcradmin gentones genwave

sbin_PROGRAMS = lcr genrc genextension

if ENABLE_ASTERISK_CHANNEL_DRIVER
noinst_PROGRAMS = chan_lcr.so
chan_lcr_so_SOURCES =
chan_lcr_so_LDFLAGS = --shared
chan_lcr_so_LDADD = chan_lcr.po options.po callerid.po select.po

# List chan_lcr specific sources for make dist
EXTRA_chan_lcr_so_SOURCES = chan_lcr.c chan_lcr.h


chan_lcr.po: chan_lcr.c chan_lcr.h
	$(CC) $(INCLUDES) $(AST_CFLAGS) $(CPPFLAGS) $(CFLAGS) -D_GNU_SOURCE -fPIC -c $< -o $@

callerid.po: callerid.c callerid.h
	$(CC) $(INCLUDES) -D_GNU_SOURCE $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

options.po: options.c options.h
	$(CC) $(INCLUDES) -D_GNU_SOURCE $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

select.po: select.c select.h
	$(CC) $(INCLUDES) -D_GNU_SOURCE $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

install-exec-hook: chan_lcr.so
	$(INSTALL) -d '$(DESTDIR)$(astmoddir)'
	$(INSTALL) chan_lcr.so '$(DESTDIR)$(astmoddir)'

uninstall-hook:
	cd '$(DESTDIR)$(astmoddir)' && rm -f chan_lcr.so
endif

INCLUDES = $(all_includes) -I$(top_srcdir)/include $(MISDN_INCLUDE) $(GSM_INCLUDE) $(SS5_INCLUDE) $(SIP_INCLUDE) -Wall $(INSTALLATION_DEFINES)

lcr_SOURCES = \
	main.c select.c trace.c options.c tones.c alawulaw.c cause.c interface.c message.c callerid.c socket_server.c \
	port.cpp vbox.cpp remote.cpp \
	$(MISDN_SOURCE) $(GSM_SOURCE) $(SS5_SOURCE) $(SIP_SOURCE) \
	endpoint.cpp endpointapp.cpp \
	appbridge.cpp apppbx.cpp route.c action.cpp action_efi.cpp action_vbox.cpp extension.c mail.c \
	join.cpp joinpbx.cpp

lcr_LDADD = $(LIBCRYPTO) $(MISDN_LIB) -lpthread $(GSM_LIB) $(SIP_LIB)


lcradmin_SOURCES = lcradmin.c cause.c options.c
genextension_SOURCES = genext.c options.c extension.c


# List all headers for make dist
noinst_HEADERS = \
	main.h macro.h select.h trace.h options.h tones.h alawulaw.h cause.h interface.h \
	message.h callerid.h socket_server.h port.h vbox.h endpoint.h endpointapp.h \
	appbridge.h apppbx.h route.h extension.h join.h joinpbx.h lcrsocket.h

noinst_HEADERS += myisdn.h mISDN.h dss1.h crypt.h remote.h
noinst_HEADERS += ss5.h ss5_encode.h ss5_decode.h
noinst_HEADERS += mncc.h gsm.h gsm_audio.h gsm_bs.h gsm_ms.h
noinst_HEADERS += ie.cpp sip.h


# Special install function to preserve existing configs.
# Optimization with equivalen results are welcome
CONFIGFILES = options.conf interface.conf routing.conf directory.list
TONEDIRS = tones_american tones_efi tones_german vbox_english vbox_german

EXTRA_DIST = default debian $(TONEDIRS)

install-data-hook:
	@fns='strcpy strncpy strcat strncat sprintf snprintf' ; \
	files=$$( find $(srcdir) -type f -name "*.c*" \
	  | $(GREP) -v -e chan_lcr.c -e callerid.c ) ; \
	test -z "$$files" || { for fn in $$fns ; do \
	  $(GREP) -n $$fn $$files ; if test $$? = 0 ; then \
	    ( echo "dont use $$fn, use makro instead." ; exit -1 ) ; \
	  fi ; \
	done ; }
	mkdir -p '$(DESTDIR)$(CONFIGdir)'
	mkdir -p '$(DESTDIR)$(SHAREdir)'
	mkdir -p '$(DESTDIR)$(LOGdir)'
	mkdir -p '$(DESTDIR)$(EXTENSIONdir)'
	@fs='$(CONFIGFILES)' ; for f in $$fs ; do \
	  if test -a "$(DESTDIR)$(CONFIGdir)/$$f" ; then \
	    echo "NOTE: $$f already exists, not changed." ; \
	  else $(INSTALL) -m 644 $(srcdir)/default/$$f "$(DESTDIR)$(CONFIGdir)/" ; fi ; \
	done
	@if test -a "$(DESTDIR)$(CONFIGdir)/numbering_int.conf" ; then \
		echo "NOTE: numbering_int.conf is obsolete, please use routing." ; fi
	@if test -a "$(DESTDIR)$(CONFIGdir)/numbering_ext.conf" ; then \
		echo "NOTE: numbering_ext.conf is obsolete, please use routing." ; fi
	@ds='$(TONEDIRS)' ; for d in $$ds ; do \
	  destdir="$(DESTDIR)$(SHAREdir)/$$d" ; \
	  if test -a "$$destdir" ; then \
	    echo "NOTE: $$d already exists, not overwritten." ; \
	  else mkdir -p "$$destdir" ; cp -a $(srcdir)/$$d/* "$$destdir/" ; fi ; \
	done


# Because distuninstallcheck expects relative filenames, we cannot
# replace './' with $(prefix) in _listfiles. Therefore we do it the
# other way around to filter out configuration files and tones.
distcheck_CONFIGdir = echo $(CONFIGdir) | sed 's|^$(prefix)/|./|'
distcheck_SHAREdir  = echo $(SHAREdir)  | sed 's|^$(prefix)/|./|'

distuninstallcheck_listfiles = find . -type f -print \
	| $(GREP) -v "$$( $(distcheck_CONFIGdir) )" \
	| $(GREP) -v "$$( $(distcheck_SHAREdir) )"


clean-generic:
	rm -f *.po

# test rule, nothing important
#echo:
#	-echo $(all_libraries) >&2

# CAUTION: CRUDE CRUDE HACK !!
# This transforms all compiling and linking calls from gcc into g++
# This should better be removed and all .c files renamed to .cpp
COMPILE=$(CXXCOMPILE)
LINK=$(CXXLINK)


