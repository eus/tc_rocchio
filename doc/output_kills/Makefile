.PHONY = all clean mrproper

CXX_EXECUTABLES := $(subst .cpp,,$(wildcard reader_vec_*.cpp))
OBJECTS := $(subst .cpp,.o,$(wildcard reader_vec_*.cpp))

COMMON_COMPILER_FLAGS := -Wall -O3

CPPFLAGS := -DBUFFER_SIZE=4096 $(if $(TESTING), -DTESTING)
CFLAGS := $(COMMON_COMPILER_FLAGS)
CXXFLAGS := -std=c++0x $(COMMON_COMPILER_FLAGS)

all: $(C_EXECUTABLES) $(CXX_EXECUTABLES)

$(CXX_EXECUTABLES): %: %.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

$(OBJECTS): ../../utility.h ../../utility_vector.h

clean:
	-rm -- $(OBJECTS) > /dev/null 2>&1

mrproper: clean
	-rm -- $(C_EXECUTABLES) $(CXX_EXECUTABLES) > /dev/null 2>&1
