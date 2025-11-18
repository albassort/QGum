CC := clang 
python-version := $(shell pkg-config --libs python3-embed)
cflags :=  -Wall -Wextra -Wswitch -Wswitch-enum -Wimplicit-fallthrough  -Wpedantic
include := -Ideps/mlib -Ideps/clog

parsing := ./src/parsing
runtime := ./src/runtime
python := ./src/python
shared := ./src/shared

link :=


ifeq ($(release),true)
cflags += -O3 -DNDEBUG

else

cflags += -g -O0 -DDEBUG 
qgum: cflags += -fsanitize=address,undefined 

endif


.PHONY: clean 

all: qgum

define check_lib =
	@ldconfig -p | grep $(1).so || { echo "lib $(1) not found. You need to install it to compile"; exit 1; }
	$(eval link:= -l$(1))
endef

py: include +=  $(shell python3-config --includes)
py: link +=  $(shell python3-config --libs)
py: link +=  $(python-version)

py: test.c  
	$(CC) test.c $(link) $(include) $(cflags) -o $@
	

$(parsing)/valid_keys.h: $(parsing)/valid_keys.json
	rm -f $(parsing)/valid_keys.h\
	cd $(parsing) && echo '#ifndef VALID_KEYS_JSON\n#define VALID_KEYS_JSON\n' > valid_keys.h
	cd $(parsing) && xxd -i valid_keys.json >> valid_keys.h
	cd $(parsing) &&  echo '#endif' >> valid_keys.h

valid_keys: $(parsing)/valid_keys.h

build/qgum: cflags += -lclex
build/qgum: cflags += -lfl

build/lex.o: $(parsing)/qgum.l
	cd $(parsing) && flex qgum.l
	clang -c $(parsing)/lex.yy.c -o build/lex.o

build/qgum: ./src/main.c $(parsing)/parser.c $(parsing)/valid_keys.h $(parsing)/parser.h build/lex.o
	$(call check_lib,jansson)
	$(CC) ./src/main.c $(parsing)/parser.c build/lex.o $(include) $(cflags) $(link) -o $@

qgum: build/qgum

clean:
	rm build/* || true

