CC = gcc

CFLAGS  = -O0 -g -Wall -pthread
LDFLAGS =
INCLUDES = -I./mINI.c \
	   $(shell pkg-config --cflags \
	     gdk-pixbuf-2.0 \
	     glib-2.0 \
	     libpng16 \
	     mount \
	     blkid \
	     libgpiod \
	     libsystemd \
	     libnotify \
	   )
LIBS = $(shell pkg-config --libs \
	     gdk-pixbuf-2.0 \
	     glib-2.0 \
	     libpng16 \
	     mount \
	     blkid \
	     libgpiod \
	     libsystemd \
	     libnotify \
	   )

MAIN = cartridged.elf

SRCS = main.c log.c detection.c unit.c notify.c config.c mINI.c/mini.c
OBJS = $(SRCS:.c=.o)

BINDIR ?= /usr/local/bin

.PHONY: depend clean install

all:    $(MAIN)
	@echo compile $(MAIN)

install:
	@echo "Installing binary..."
	@install -m 557 $(MAIN) $(BINDIR)
	@echo "Installing systemd service..."
	@mkdir -p /etc/cartridged/
	@install -m 644 ./etc/cartridged/config.ini /etc/cartridged/
	@install -m 644 ./etc/systemd/system/cartridged.service /etc/systemd/system/
	@echo "Reloading systemd..."
	@systemctl daemon-reload

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)
        

