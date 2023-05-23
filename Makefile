CC = gcc
LDFLAGS = -lm

SRC = lisp.c mpc.c
EXECUTABLE = lisp

all: $(EXECUTABLE)

$(EXECUTABLE): $(SRC)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(EXECUTABLE)

