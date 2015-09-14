
SRCDIR = src
OBJDIR = obj
OBJFULL = obj/*.o

CC=clang
CFLAGS= -g -Iinc -IjustGarble/include
LIBS=-lmsgpack -march=native -maes -msse4 -lm -lrt -lcrypto -lssl -lgmp

SOURCES := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS := $(wildcard $(SRCDIR)/*.o)

JUSTGARBLE := $(wildcard justGarble/src/*.c)
JUSTGARBLEOBJ := $(wildcard justGarble/obj/*.o)

all: $(OBJECTS)
	$(CC) $(JUSTGARBLE) $(SOURCES) $(CFLAGS) $(LIBS)

.PHONEY: clean
clean:
