ACLOCAL_AMFLAGS = -I m4

# --- library -----------------------------------------------------------

lib_LTLIBRARIES = libplcode.la

libplcode_la_SOURCES = \
	src/plcode_tables.c \
	src/plcode_golay.c \
	src/plcode_ctcss_enc.c \
	src/plcode_ctcss_dec.c \
	src/plcode_dcs_enc.c \
	src/plcode_dcs_dec.c \
	src/plcode_dtmf_enc.c \
	src/plcode_dtmf_dec.c \
	src/plcode_cwid_enc.c \
	src/plcode_cwid_dec.c \
	src/plcode_mcw_enc.c \
	src/plcode_mcw_dec.c \
	src/plcode_fskcw_enc.c \
	src/plcode_fskcw_dec.c \
	src/plcode_twotone_enc.c \
	src/plcode_twotone_dec.c \
	src/plcode_selcall_enc.c \
	src/plcode_selcall_dec.c \
	src/plcode_toneburst_enc.c \
	src/plcode_toneburst_dec.c \
	src/plcode_mdc1200_enc.c \
	src/plcode_mdc1200_dec.c \
	src/plcode_courtesy_enc.c \
	src/plcode_tone_enc.c

libplcode_la_CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I$(top_srcdir)/include -I$(top_srcdir)/src
libplcode_la_LDFLAGS = -version-info 0:0:0
libplcode_la_LIBADD = -lm

# --- public header ------------------------------------------------------

plcodeincludedir = $(includedir)
plcodeinclude_HEADERS = include/plcode.h

# --- pkg-config ---------------------------------------------------------

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = plcode.pc

# --- tests --------------------------------------------------------------

check_PROGRAMS = test_plcode

test_plcode_SOURCES = \
	tests/test_main.c \
	tests/test_golay.c \
	tests/test_ctcss.c \
	tests/test_dcs.c \
	tests/test_dtmf.c \
	tests/test_cwid.c \
	tests/test_mcw.c \
	tests/test_fskcw.c \
	tests/test_twotone.c \
	tests/test_selcall.c \
	tests/test_toneburst.c \
	tests/test_mdc1200.c \
	tests/test_courtesy.c

test_plcode_CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -I$(top_srcdir)/include -I$(top_srcdir)/src
test_plcode_LDADD = libplcode.la -lm

TESTS = test_plcode

# --- backward-compat alias ---------------------------------------------

test: check
.PHONY: test
