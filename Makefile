.PHONY: server clean
CFLAGS=-g -O0

server: $(patsubst %.c, %.o, $(wildcard src/*.c))
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) src/*.o

test:
	$(MAKE) CFLAGS="$(CFLAGS) -fsanitize=address" server
	./server
	