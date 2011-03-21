.PHONY = all clean mrproper

C_EXECUTABLES :=
CXX_EXECUTABLES := check_binary_classifiers
OBJECTS := check_binary_classifiers.o

COMMON_COMPILER_FLAGS := -Wall $(if $(DONT_OPTIMIZE),-g3,-O3) -I ..

DEBUGGING := $(if $(DEBUG),,-DNDEBUG) $(if $(BE_VERBOSE),-DBE_VERBOSE)
CPPFLAGS := $(DEBUGGING) -DBUFFER_SIZE=4096 -DOS_PATH_DELIMITER=\'/\'
CFLAGS := $(COMMON_COMPILER_FLAGS)
CXXFLAGS := -std=c++0x $(COMMON_COMPILER_FLAGS)

all: $(C_EXECUTABLES) $(CXX_EXECUTABLES)

$(CXX_EXECUTABLES): %: %.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

check_binary_classifiers.o: ../utility.h ../utility.hpp ../utility_idf_dic.hpp \
	../utility_vector.hpp

clean:
	-rm -- $(OBJECTS) > /dev/null 2>&1

mrproper: clean
	-rm -- $(C_EXECUTABLES) $(CXX_EXECUTABLES) > /dev/null 2>&1
