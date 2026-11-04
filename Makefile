# Synth City
#   make            -> nativer Build (Linux-Desktop oder direkt auf dem R36S)
#   make r36s       -> Cross-Build fuer ArkOS (aarch64), siehe tools/cross-build.sh
CXX      ?= g++
CC_LINK  ?= gcc
SDL_CFLAGS ?= $(shell sdl2-config --cflags)
SDL_LIBS   ?= $(shell sdl2-config --libs)
CXXFLAGS += -DWITH_MIDI -O2 -fno-math-errno -Wall -Wextra -Wno-unused-parameter -fno-exceptions -fno-rtti -fno-threadsafe-statics $(SDL_CFLAGS)
LDFLAGS  +=
# auf arm64 (R36S/RK3326 bzw. CI-Runner fuer das Groovebox-OS) fuer den In-Order-Kern Cortex-A35 optimieren
ifeq ($(shell uname -m),aarch64)
CXXFLAGS += -mcpu=cortex-a35
endif
LIBS     ?=
# make DRM=1 -> direkte DRM-Ausgabe ohne GPU (Groovebox-OS), braucht libdrm-dev
ifeq ($(DRM),1)
CXXFLAGS += -DWITH_DRM $(shell pkg-config --cflags libdrm)
LIBS     += -ldrm
endif
# make SAN=1 -> AddressSanitizer + UBSan (Tests: make SAN=1 && ./groovebox --selftest)
ifeq ($(SAN),1)
TFLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1
CXXFLAGS += $(TFLAGS)
LDFLAGS += $(TFLAGS)
LIBS += -lstdc++
endif
SRC = src/main.cpp src/engine.cpp src/input.cpp src/samples.cpp src/midi.cpp src/dhcpd.cpp src/web.cpp src/webpage.cpp src/drmout.cpp src/usbaudio.cpp src/remote.cpp
OBJ = $(SRC:.cpp=.o)
OUT ?= groovebox

$(OUT): $(OBJ)
	$(CC_LINK) $(LDFLAGS) -o $@ $(OBJ) $(SDL_LIBS) $(LIBS) -lasound -lm -lpthread

src/%.o: src/%.cpp src/*.h src/*.inc
	$(CXX) $(CXXFLAGS) -c $< -o $@

r36s:
	./tools/cross-build.sh

clean:
	rm -f $(OBJ) groovebox groovebox-aarch64 tests/unit tests/unit-tsan
.PHONY: clean r36s

# Tests: make test (SAN=1 -> AddressSanitizer + UBSan), make test-tsan (ThreadSanitizer: Mehrkern-Audio)
tests/unit: tests/unit.cpp src/engine.cpp src/samples.cpp src/*.h
	$(CXX) $(filter-out -O2,$(CXXFLAGS)) -O2 $(TFLAGS) -Isrc -o $@ tests/unit.cpp src/engine.cpp src/samples.cpp $(TFLAGS) -lm -lpthread $(if $(TFLAGS),-lstdc++,)
test: tests/unit
	./tests/unit samples
test-tsan:
	$(CXX) -O1 -g -fsanitize=thread -DWITH_MIDI -fno-exceptions -fno-rtti -Isrc -o tests/unit-tsan tests/unit.cpp src/engine.cpp src/samples.cpp -lm -lpthread
	TSAN_OPTIONS=halt_on_error=1 ./tests/unit-tsan samples
.PHONY: test test-tsan
