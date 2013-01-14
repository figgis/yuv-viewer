DBG        = #-ggdb3
OPTFLAGS   = -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes $(DBG) -pedantic
SDL_LIBS   := $(shell sdl-config --static-libs)
SDL_CFLAGS := $(shell sdl-config --cflags)
CFLAGS     = $(OPTFLAGS)  $(SDL_CFLAGS) -std=c99
LDFLAGS    = $(SDL_LIBS) #-lefence

SRC        = yv.c
TARGET     = yv
OBJ        = $(SRC:.c=.o)

default: $(TARGET)

%.o: %.c Makefile
	$(CC) $(CFLAGS)  -c -o $@ $<

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm $(OBJ) $(TARGET)
