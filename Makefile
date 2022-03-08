CC = gcc

CFLAGS = -std=c99 -g -Wall

LIBS = -ledit -lgc -ltcc
FFI_LIBS = -ldl -lffi

SRC = reader.c printer.c types.c env.c core.c compiler.c
HEADERS = reader.h printer.h types.h env.h core.h

LIB_DIR = ./libs

LIB_H = $(LIB_DIR)/list/src/list.h
LIB_H += $(LIB_DIR)/hashmap/src/hashmap.h
LIB_H += $(LIB_DIR)/vector/src/vector.h
LIB_H += $(LIB_DIR)/iterator/iterator.h

LIB_SRC = $(LIB_DIR)/list/src/list.c
LIB_SRC += $(LIB_DIR)/hashmap/src/hashmap.c
LIB_SRC += $(LIB_DIR)/vector/src/vector.c
LIB_SRC += $(LIB_DIR)/iterator/iterator.c

C2_SRC = c2.c $(SRC) $(LIB_SRC)
C2_HEADERS = $(HEADERS) $(LIB_H)
C2 = c2

build: $(C2)

all: build test perf

$(C2): $(C2_SRC) $(C2_HEADERS)
	$(CC) $(CFLAGS) $(C2_SRC) $(LIBS) $(FFI_LIBS) -DWITH_FFI -o $(C2)
	@echo '--'

test: $(C2)
	@echo "TEST resuts for *ARGV*";
	@./tests/argv/run_argv_test.sh ./c2 | egrep 'FAIL|OK'
	@echo '--'
	@./run-tests.sh | grep -A4 'TEST RESULTS'
	@echo '--'


perf: $(C2)
	@echo "TEST results for performance";
	@echo '----------------------------'
	@echo 'Running: /tests/perf/perf1.mal'
	@./run ./tests/perf/perf1.mal
	@echo '--'
	@echo 'Running: /tests/perf/perf2.mal'
	@./run ./tests/perf/perf2.mal
	@echo '--'
	@echo 'Running: /tests/perf/perf3.mal'
	@./run ./tests/perf/perf3.mal
	@echo '--'

clean:
	@echo "Cleaning..."
	@rm -f $(C2)

.PHONY clean:
.PHONY test:
.PHONY perf:
.PHONY all:
