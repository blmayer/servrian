.PHONY: servrian clean test debug test_hash
CFLAGS=-g -O0 -Wall -Wextra -fsanitize=address -fsanitize=bounds

servrian: $(patsubst %.c,%.o,$(wildcard src/*.c))
	[ -e bin ] || mkdir bin
	$(CC) $(CFLAGS) $^ -o bin/$@

release:
	$(MAKE) CFLAGS="-Ofast -static" servrian

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) src/*.o
	$(RM) bin/*

test: test_hash
	./bin/$<

test_hash: src/test/test_hash.c src/aux.c
	$(CC) $(CFLAGS) $^ -o bin/$@

debug:
	$(MAKE) CFLAGS="$(CFLAGS) -fsanitize=address" servrian
	./servrian
	
