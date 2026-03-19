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
          src/plcode_cwid_dec.c

OBJ     = $(SRC:.c=.o)

TEST_SRC = tests/test_main.c \
           tests/test_golay.c \
           tests/test_ctcss.c \
           tests/test_dcs.c \
           tests/test_dtmf.c \
           tests/test_cwid.c

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
