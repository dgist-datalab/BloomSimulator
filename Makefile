all: simulator

CC = g++

CFLAGS=\
#	   -O3\
#   -fsanitize=address\

SRCS +=\
	bloom_calc.c\
	bloomfilter.c\
	reclaim.c\

TARGETOBJ=\
		$(patsubst %.c, %.o, $(SRCS))\


simulator: $(TARGETOBJ) main.c
	$(CC) -g $(CFLAGS) -o $@ $^ -lm


.c.o:
	$(CC) -g $(CFLAGS) -c $< -o $@ -lm

clean:
	@$(RM) simulator
	@$(RM) *.o

