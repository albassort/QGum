CC := clang -x c
cflags :=  -Wall -Wextra -Wswitch -Wswitch-enum -Wimplicit-fallthrough -g
include := -Ideps/mlib -Ideps/clog
link :=

.PHONY: all clean py

all: py parser

py: link += -lpython3.12 
py: link +=  $(shell python3-config --cflags )
py: make-headers

py:  
	$(CC) test.c $(link) $(cflags) -o $@
	
parser: cflags += -fsanitize=address,undefined 

parser: link += -ljansson

valid_keys.h: valid_keys.json
	xxd -i valid_keys.json > $@

parser: parser.c valid_keys.h
	$(CC) parser.c $(include) $(cflags) $(link) -o $@


