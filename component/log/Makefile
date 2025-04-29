CC = gcc
CFLAGS = -Wall -Wextra -Ifilter -g
LDFLAGS = -lz

SRC = channel.c \
	  filter/filter.c \
	  filter/procname_filter.c \
	  input_channel_socket.c \
	  input_channel_timer.c \
	  input_channel_signal.c \
	  output_channel_console.c \
	  log_api.c log_server.c

OBJ = $(SRC:.c=.o)

all: log_server liblog_api.a test
log_server: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

liblog_api.a: log_api.o
	ar rcs $@ $^

test: test.o liblog_api.a
	$(CC) -o $@  $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) log_server liblog_api.a

.PHONY: all clean
