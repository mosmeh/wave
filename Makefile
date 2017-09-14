CXX := g++
CXXFLAGS := -std=c++11 -Wall -O2
INCDIR := -Iinclude/ -Iinclude/glad/
LDFLAGS := -lglfw
TARGETS := wave
OBJS := src/glad.o src/main.o

.PHONY: all clean
.SUFFIXES: .c .cpp .o

all: $(TARGETS)

.c.o:
	$(CXX) $(CXXFLAGS) $(INCDIR) $^ -c -o $@ $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) $(INCDIR) $^ -c -o $@ $(LDFLAGS)

wave: $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCDIR) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) $(TARGETS) $(OBJS)
