
.PHONY: all vie clean

all: vie

vie: vie.c
	$(CC) vie.c -o vie.out -Wall -Wextra -pedantic -std=c99

clean:
	rm -rf *.o vie.out
