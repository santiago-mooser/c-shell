CC=gcc
EXE=shell

all: $(EXE)

shell: shell.c
	$(CC) shell.c -o $@

clean:
	rm $(EXE)
