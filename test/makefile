CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

ROOT_DIR = ..
SRC_DIR = $(ROOT_DIR)\src
LIB_DIR = $(ROOT_DIR)\lib
INCLUDE_DIR = $(SRC_DIR)

LIB_NAME = RingBuffer
LIB_FILE = $(LIB_DIR)\lib$(LIB_NAME).a

TEST_DIRS = $(patsubst %/,%,$(patsubst ./%,%,$(wildcard */)))

.DEFAULT_GOAL := help

$(TEST_DIRS): %:
	@if not exist "$(LIB_FILE)" ( \
		echo Error: Library $(LIB_FILE) not found. Please build the library first. && \
		exit 1 \
	)
	@if not exist "$@\main.c" ( \
		echo Error: $@\main.c not found && \
		exit 1 \
	)
	@echo Building test: $@
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $@\main.c -L$(LIB_DIR) -l$(LIB_NAME) -o $@\$@.exe
	@echo Build completed: $@\$@.exe

run:
	@if "$(TEST)"=="" ( \
		echo Error: Please specify a test to run. Usage: make run testname && \
		echo Available tests: $(TEST_DIRS) && \
		exit 1 \
	)
	@if not exist "$(TEST)\$(TEST).exe" ( \
		echo $(TEST).exe not found. Building it first... && \
		$(MAKE) $(TEST) \
	)
	@echo Running test: $(TEST)
	@cd $(TEST) && .\$(TEST).exe

all:
	@for %%t in ($(TEST_DIRS)) do ( \
		if exist "%%t\main.c" ( \
			echo Building test: %%t && \
			$(CC) $(CFLAGS) -I$(INCLUDE_DIR) %%t\main.c -L$(LIB_DIR) -l$(LIB_NAME) -o %%t\%%t.exe && \
			echo Build completed: %%t\%%t.exe \
		) else ( \
			echo Skipping %%t: main.c not found \
		) \
	)

run-all:
	@for %%t in ($(TEST_DIRS)) do ( \
		if exist %%t\%%t.exe ( \
			echo Running test: %%t && \
			cd %%t && .\%%t.exe && \
			cd .. && \
			echo ---------------------------------------- \
		) \
	)

clean:
	@for %%t in ($(TEST_DIRS)) do ( \
		if exist "%%t\%%t.exe" ( \
			echo Deleting %%t\%%t.exe... && \
			del /q "%%t\%%t.exe" \
		) \
	)
	@echo Clean completed

list:
	@echo Discovered test directories:
	@for %%t in ($(TEST_DIRS)) do (if exist "%%t\main.c" (@echo + %%t) else (@echo - %%t))

help:
	@echo Available targets:
	@echo   all                    - Build all tests
	@echo   run-all                - Build and run all tests
	@echo   testname               - Build specific test
	@echo   run TEST=testname      - Build and run specific test
	@echo   clean                  - Remove all generated test executables
	@echo   list                   - List all discovered test directories
	@echo   help                   - Show this help message
	@echo.
	@echo $(TEST_DIRS)

.PHONY: all run run-all clean help list $(TEST_DIRS)

%:
	@: