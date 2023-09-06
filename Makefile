CC			=	gcc
CFLAGS		=	-Wall -pedantic -g
LIBFLAGS	=	-pthread -lrt -lm
VALFLAGS	=	-s --leak-check=full --trace-children=yes

all: exam

exam: exam.c unbQueue.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBFLAGS)

#test1 executes the programme from the current directory with 1 worker (W)
test1:	exam
	./exam . 1

#test2 executes the programme from the provadir directory with 5 workers (W)
test2:	exam
	./exam provadir 5

#test3 checks that the programme doesn't have any memory leak using valgrind; it starts from the current directory with 5 workers (W)
test3: exam
	valgrind $(VALFLAGS) ./exam . 5

clean:
	rm -f exam

.PHONY: clean test1 test2 test3 all