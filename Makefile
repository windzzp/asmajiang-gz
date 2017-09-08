#rocenting@gmail.com
CC = gcc
CXX = g++
CFLAGS = -g -O0 -Wall
CXXFLAGS = -g -O0 -fexceptions -Wall
INCPATH = -I/usr/local/include -I./libmajiang
LD = gcc
LFLAGS =
LIBS = -L. -L./libmajiang -L/usr/local/lib -lmajiang -lstdc++ -lev -ljsoncpp -lhiredis -lgame

STATIC_LIB = libgame.a
OBJECTS_LIB = zjh.o client.o daemonize.o log.o log_agent.o player.o redis_client.o game.o jpacket.o

TARGET_APP = asmajiang
TARGET_OBJS = table.o table_delegate.o replay.o

all:$(TARGET_APP)

$(TARGET_APP): $(TARGET_OBJS)
	$(LD) $(LFLAGS) -o $(TARGET_APP) $(TARGET_OBJS) $(LIBS)
 

lib: $(STATIC_LIB)

$(STATIC_LIB): $(OBJECTS_LIB)
	ar -r $(STATIC_LIB) $(OBJECTS_LIB)

####### Implicit rules ######
%.d:%.cc
	@set -e; \
	rm -f $@; \
	$(CC) -MM $< $(INCLUDEFLAGS) > $@.$$$$ $(INCPATH); \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

-include $(OBJECTS:.o=.d)

.SUFFIXES: .cpp .cc .cxx .c
.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

#.PHONY:clean
clean:
	rm -rf  *.o
