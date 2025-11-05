CC := clang
cflags :=  -Wall -Wextra -Wswitch -Wswitch-enum
include := -Ideps/mlib -Ideps/clog

.PHONY: all clean

py: link += -lpython3.12 
py: link +=  $(shell python3-config --cflags )

py: FORCE 
	$(CC) test.c $(link) $(cflags) -o py 
	
parser: cflags += -fsanitize=address,undefined 
parser: FORCE
	$(CC) parser.c $(include) $(cflags) -o parser

FORCE: ;
