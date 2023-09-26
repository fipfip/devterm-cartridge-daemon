CC = gcc

CFLAGS  = -O0 -g -Wall -pthread
LDFLAGS =
INCLUDES = -I./mINI.c \
	   -I/usr/include/gdk-pixbuf-2.0 \
	   -I/usr/include/glib-2.0 \
	   -I/usr/lib/glib-2.0/include \
	   -I/usr/include/sysprof-4 \
	   -I/usr/include/libpng16 \
	   -I/usr/include/libmount \
	   -I/usr/include/blkid
LIBS = -lwiringPi -lm -lpthread -lrt -lgpiod -lsystemd \
       -lnotify -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0

MAIN = devterm_extcart_daemon.elf

SRCS = main.c log.c detection.c unit.c notify.c mINI.c/mini.c
OBJS = $(SRCS:.c=.o)

.PHONY: depend clean

all:    $(MAIN)
	@echo compile $(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)
        

