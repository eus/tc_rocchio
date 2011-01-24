.PHONY = all clean mrproper

ifeq ($(ONLY_FOR_ME),yes)
ARCHITECTURE_DEPENDENT_OPTIMIZATION := \
	-march=native -mfpmath=sse -malign-double -mmmx -msse -msse2 -msse3
endif

C_EXECUTABLES := tokenizer  reader_2col streaming_tokenizer reader_vec
CXX_EXECUTABLES := tf idf_dic w_to_vector rocchio classifier
OBJECTS := tokenizer.o tf.o idf_dic.o reader_2col.o streaming_tokenizer.o \
	w_to_vector.o reader_vec.o rocchio.o classifier.o

COMMON_COMPILER_FLAGS := -Wall -O3 $(ARCHITECTURE_DEPENDENT_OPTIMIZATION)

CPPFLAGS := -DBUFFER_SIZE=4096
CFLAGS := $(COMMON_COMPILER_FLAGS)
CXXFLAGS := -std=c++0x $(COMMON_COMPILER_FLAGS)

all: $(C_EXECUTABLES) $(CXX_EXECUTABLES)

$(CXX_EXECUTABLES): %: %.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

tokenizer.o: utility.h
tokenizer:

streaming_tokenizer.o: utility.h
streaming_tokenizer:

reader_2col.o: utility.h
reader_2col:

reader_vec.o: utility.h
reader_vec:

tf.o: utility.h
idf_dic.o: utility.h
w_to_vector.o: utility.h utility_vector.h
rocchio.o: utility.h utility_vector.h utility.hpp
classifier.o: utility.h utility_vector.h utility.hpp

clean:
	-rm -- $(OBJECTS) > /dev/null 2>&1

mrproper: clean
	-rm -- $(C_EXECUTABLES) $(CXX_EXECUTABLES) > /dev/null 2>&1
