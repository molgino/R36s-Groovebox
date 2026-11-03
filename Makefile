# R36S Groovebox
#   make            -> nativer Build (Linux-Desktop oder direkt auf dem R36S)
#   make r36s       -> Cross-Build fuer ArkOS (aarch64), siehe tools/cross-build.sh
CXX      ?= g++
CC_LINK  ?= gcc
SDL_CFLAGS ?= $(shell sdl2-config --cflags)
SDL_LIBS   ?= $(shell sdl2-config --libs)
CXXFLAGS += -DWITH_MIDI -O2 -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti -fno-threadsafe-statics $(SDL_CFLAGS)
LDFLAGS  +=
SRC = src/main.cpp src/engine.cpp src/input.cpp src/samples.cpp src/midi.cpp src/dhcpd.cpp src/web.cpp src/webpage.cpp
OBJ = $(SRC:.cpp=.o)
OUT ?= groovebox

$(OUT): $(OBJ)
	$(CC_LINK) $(LDFLAGS) -o $@ $(OBJ) $(SDL_LIBS) -lasound -lm

src/%.o: src/%.cpp src/*.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

r36s:
	./tools/cross-build.sh

clean:
	rm -f $(OBJ) groovebox groovebox-aarch64
.PHONY: clean r36s
