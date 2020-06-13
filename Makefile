CC = gcc
LD = gcc
OBJS = main.o network.o http.o logger.o config_parser.o w3c_log.o utils.o
DEPS = network.h http.h logger.h config_parser.h w3c_log.h utils.h
TARGET = server
CFLAGS = -Wall -Werror

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) -o $@ $^ $(CFLAGS)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o $(TARGET)
