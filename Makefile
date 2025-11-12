CC := clang -x c

python-version := $(shell pkg-config --libs python3-embed)

cflags :=  -Wall -Wextra -Wswitch -Wswitch-enum -Wimplicit-fallthrough  -Wpedantic
include := -Ideps/mlib -Ideps/clog
link :=


ifeq ($(release),true)

cflags += -O3 -DNDEBUG

else

cflags += -g -O0 -DDEBUG 
parser: cflags += -fsanitize=address,undefined 

endif


.PHONY: all clean 

all: py parser

define check_lib =
	@ldconfig -p | grep $(1).so || { echo "lib $(1) not found. You need to install it to compile"; exit 1; }
	$(eval link:= -l$(1))
endef

py: include +=  $(shell python3-config --includes)
py: link +=  $(shell python3-config --libs)
py: link +=  $(python-version)

py: test.c  
	$(CC) test.c $(link) $(include) $(cflags) -o $@
	

valid_keys.h: valid_keys.json
	xxd -i valid_keys.json > $@

parser: parser.c valid_keys.h
	$(call check_lib,jansson)
	$(CC) parser.c $(include) $(cflags) $(link) -o $@

clean:
	rm parser
	rm py
	rm valid_keys.h

