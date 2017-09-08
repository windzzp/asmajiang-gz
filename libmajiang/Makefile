#rocenting@gmail.com
CC = clang
CXX = g++
CFLAGS = -g -O0 -Wall
CXXFLAGS = -g -O0 -fexceptions -Wall
INCPATH = #-I/usr/include/mysql
LD = g++
LFLAGS = -g
LIBS = -L. -lstdc++

STATIC_LIB = libmajiang.a
OBJECTS_LIB = card.o deck.o hole_cards.o

DESTDIR_TARGET = test
OBJECTS = test.o $(OBJECTS_LIB)

all: $(DESTDIR_TARGET)

$(DESTDIR_TARGET): $(OBJECTS)
	$(LD) $(LFLAGS) -o $(DESTDIR_TARGET) $(OBJECTS) $(LIBS)

lib: $(STATIC_LIB)

$(STATIC_LIB): $(OBJECTS_LIB)
	ar -r $(STATIC_LIB) $(OBJECTS_LIB)

####### Implicit rules ######

.SUFFIXES: .cpp .cc .cxx .c
.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cc.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

clean:
	rm -rf $(DESTDIR_TARGET) *.o libmajiang.a
