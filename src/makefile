#obj = $(wildcard *.o)
obj = main.o

shared = $(wildcard ../lib/*.so)
shared_name = $(patsubst ../lib/lib%.so, %, $(shared))
link_shared = $(patsubst %, -l %, $(shared_name))

lib = $(wildcard ../lib/*.a)
lib_name = $(patsubst ../lib/lib%.a, %, $(lib))
link_static = $(patsubst %, -l%, $(lib_name))

lib_dir = ../lib
inc_dir = ../include

CPPFLAGS = -I $(inc_dir) -L $(lib_dir) -Wl,--whole-archive $(link_static) -Wl,--no-whole-archive $(link_shared) -lpthread -g -rdynamic -no-pie

CC = g++
target = ../build/StrategyPlatform

ALL:$(target)

$(target):$(obj)
	$(CC) $^ -o $@ $(CPPFLAGS)

#$(obj):main.cpp
#	$(CC) $^ -o $@ -I ../include


clean:
	rm $(target)


.PHONY:clean ALL


