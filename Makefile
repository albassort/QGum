CC := clang 
python-version := $(shell pkg-config --libs python3-embed)
cflags :=  -Wall -Wextra -Wswitch -Wswitch-enum -Wimplicit-fallthrough  -Wpedantic

parsing := ./src/parsing
runtime := ./src/runtime
python := ./src/python
shared := ./src/shared

include := -Ideps/mlib -Ideps/clog -I$(parsing) -I$(runtime) -I$(python) -I$(shared)

link :=


ifeq ($(release),true)
cflags += -O3 -DNDEBUG

else

cflags += -g -O0 -DDEBUG 
# qgum: cflags += -fsanitize=address,undefined 

endif


.PHONY: clean 

all: qgum

define check_lib =
	@pkg-config --libs $(1) || { echo "lib $(1) not found. You need to install it to compile"; exit 1; }
	$(eval link:= $(shell pkg-config --libs $(1)))
endef

py: test.c  
	$(CC) test.c $(link) $(include) $(cflags) -o $@
	

$clang(parsing)/valid_keys.h: $(parsing)/valid_keys.json
	rm -f $(parsing)/valid_keys.h
	cd $(parsing) && echo '#ifndef VALID_KEYS_JSON\n#define VALID_KEYS_JSON\n' > valid_keys.h
	cd $(parsing) && xxd -i valid_keys.json >> valid_keys.h
	cd $(parsing) &&  echo '#endif' >> valid_keys.h

valid_keys: $(parsing)/valid_keys.h

build/qgum: cflags += -lfl

compiled_objects :=

$(parsing)/qgum-scanner.c: $(parsing)/qgum.l
	$(call check_lib,fl)
	cd $(parsing) && flex qgum.l

	# cc -c $(parsing)/qgum-scanner.c -o build/lex.o

SRCS := $(shell find src -name '*.c')
HEADERS := $(shell find src -name '*.h')
OBJS := $(shell find src -name '*.c' | sed -e 's/src/build/g' -e 's/\.c/\.o/g')

PY_OBJS := $(filter build/python/%, $(OBJS))
NONPY_OBJS := $(filter-out build/python/%, $(OBJS))

ASAN := -fsanitize=address,undefined 

$(PY_OBJS): build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< $(include) $(cflags)  -o $@

$(NONPY_OBJS): build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< $(include) $(cflags) -o $@



build/qgum: include +=  $(shell python3-config --includes)
build/qgum: link +=  $(shell python3-config --libs)
build/qgum: link +=  $(python-version)

build/qgum: $(OBJS) $(SRCS) $(HEADERS) $(parsing)/valid_keys.h
	$(call check_lib,jansson)
	$(CC) $(OBJS) $(include) $(cflags) $(link) -o $@

qgum: build/qgum

clean:
	rm -rf build/* || true

