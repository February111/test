src = $(wildcard *.cpp)					
obj = $(patsubst %.cpp, %.o, $(src))		

src2 = $(wildcard ../application/*.cpp)					
obj2 = $(patsubst %.cpp, %.o, $(src2))		

CC = g++
CPPFLAGS = -g -rdynamic -Wall
#target = libStrategyPlatform.a 
target = ../lib/libStrategyPlatform.a 
inc = ../include2
inc2 = ../application

ALL:$(target)

$(target):$(obj) $(obj2)			
	ar rcs $@ $^

$(obj):%.o:%.cpp
	$(CC) -c $< -o $@ -I$(inc) -I$(inc2) $(CPPFLAGS)

$(obj2):%.o:%.cpp
	$(CC) -c $< -o $@ -I$(inc) -I$(inc2) $(CPPFLAGS)

clean:
	rm $(obj) $(obj2) $(target)

.PHONY:clean ALL

