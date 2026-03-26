CC      ?= cc
AR      ?= ar
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O2 -Iinclude -Isrc
LDFLAGS = -lm

LIB     = libplcode.a
TEST    = test_plcode

SRC     = src/plcode_tables.c \
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

OBJ     = $(SRC:.c=.o)

TEST_SRC = tests/test_main.c \
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

TEST_OBJ = $(TEST_SRC:.c=.o)

.PHONY: all clean test

all: $(LIB)

$(LIB): $(OBJ)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST): $(TEST_OBJ) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJ) $(LIB) $(LDFLAGS)

test: $(TEST)
	./$(TEST)

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(LIB) $(TEST)
