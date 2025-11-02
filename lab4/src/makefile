# ====================== Makefile ======================
CC       := gcc
CFLAGS   := -Wall -O2
PTHREAD  := -pthread
LDFLAGS  :=

# --- для process_memory — отключаем PIE, чтобы адреса были стабильнее
PMEM_CFLAGS  := -fno-pie
PMEM_LDFLAGS := -no-pie

# --- исходники для parallel_min_max (задание 1)
PMIN_SRCS := parallel_min_max.c find_min_max.c utils.c

# --- исходники для psum (задание 5)
SUM_HDR   := sum_lib.h
SUM_SRC   := sum_lib.c
SUM_OBJ   := sum_lib.o
SUM_LIB   := libsum.a
PSUM_SRCS := parallel_sum.c utils.c

# ------------------------------------------------------------
.PHONY: all clean run_pm run_mem run_psum

# Собрать всё
all: parallel_min_max process_memory psum

# -------- Задание 1: parallel_min_max (с --timeout) ----------
parallel_min_max: $(PMIN_SRCS)
	$(CC) $(CFLAGS) $(PMIN_SRCS) -o $@ $(LDFLAGS)

# -------- Задание 3: process_memory.c ------------------------
process_memory: process_memory.c
	$(CC) $(CFLAGS) $(PMEM_CFLAGS) process_memory.c -o $@ $(PMEM_LDFLAGS)


# -------- Задание 5: библиотека + psum -----------------------
# статическая библиотека libsum.a
$(SUM_LIB): $(SUM_OBJ)
	ar rcs $@ $^

$(SUM_OBJ): $(SUM_SRC) $(SUM_HDR)
	$(CC) $(CFLAGS) -c $(SUM_SRC) -o $(SUM_OBJ)

# исполняемый psum (линкуем с libsum.a и pthread)
psum: $(PSUM_SRCS) $(SUM_LIB) $(SUM_HDR)
	$(CC) $(CFLAGS) $(PTHREAD) $(PSUM_SRCS) -L. -lsum -o $@ $(PTHREAD)

# -------- Очистка -------------------------------------------
clean:
	rm -f parallel_min_max process_memory psum $(SUM_OBJ) $(SUM_LIB)
# ============================================================
