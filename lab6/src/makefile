CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
TARGETS = client server
LIBRARY = libfactorial.a

all: $(LIBRARY) $(TARGETS)

# Создание статической библиотеки
$(LIBRARY): factorial.o
	ar rcs $@ $^

factorial.o: factorial.c factorial.h
	$(CC) $(CFLAGS) -c factorial.c -o $@

# Клиент с линковкой библиотеки
client: client.c $(LIBRARY)
	$(CC) $(CFLAGS) -o client client.c -L. -lfactorial

# Сервер с линковкой библиотеки
server: server.c $(LIBRARY)
	$(CC) $(CFLAGS) -o server server.c -L. -lfactorial

clean:
	rm -f $(TARGETS) $(LIBRARY) *.o

test_servers:
	echo "127.0.0.1:20001" > servers.txt
	echo "127.0.0.1:20002" >> servers.txt

test: all test_servers
	@echo "Build complete! Run:"
	@echo "./server --port 20001 --tnum 2"
	@echo "./client --k 10 --mod 1000 --servers servers.txt"

.PHONY: all clean test_servers test