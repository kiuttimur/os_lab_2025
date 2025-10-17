CC=gcc
CFLAGS = -I. -Wall -O2
# ---------------------------------------------------------------
# Цель "all" — собирает все программы проекта.
# Теперь можно просто ввести `make` или `make all`
# и соберутся sequential_min_max и parallel_min_max.
# ---------------------------------------------------------------
all: sequential_min_max parallel_min_max exec_runner

sequential_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o sequential_min_max find_min_max.o utils.o sequential_min_max.c $(CFLAGS)

parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max utils.o find_min_max.o parallel_min_max.c $(CFLAGS)

# ---------------------------------------------------------------
# exec_runner — программа, запускающая sequential_min_max через exec
# Пример: ./exec_runner ./sequential_min_max --seed 10 --array_size 100 --pnum 4
# ---------------------------------------------------------------
exec_runner: exec_runner.c
	$(CC) -o exec_runner exec_runner.c $(CFLAGS)

utils.o : utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

find_min_max.o : utils.h find_min_max.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

clean :
	rm utils.o find_min_max.o sequential_min_max parallel_min_max
