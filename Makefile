.PHONY = all clean mrproper

ifeq ($(ONLY_FOR_ME),yes)
ARCHITECTURE_DEPENDENT_OPTIMIZATION := \
	-march=native -mfpmath=sse -malign-double -mmmx -msse -msse2 -msse3
endif

C_EXECUTABLES := tokenizer reader_vec
CXX_EXECUTABLES := tf idf_dic w_to_vector rocchio classifier perf_measurer
OBJECTS := tokenizer.o tf.o idf_dic.o \
	w_to_vector.o reader_vec.o rocchio.o classifier.o perf_measurer.o

COMMON_COMPILER_FLAGS := -Wall $(if $(DONT_OPTIMIZE),-g3,-O3) \
	$(ARCHITECTURE_DEPENDENT_OPTIMIZATION)

DEBUGGING := $(if $(DEBUG),,-DNDEBUG) $(if $(BE_VERBOSE),-DBE_VERBOSE)
CPPFLAGS := $(DEBUGGING) -DBUFFER_SIZE=4096 -DOS_PATH_DELIMITER=\'/\'
CFLAGS := $(COMMON_COMPILER_FLAGS)
CXXFLAGS := -std=c++0x $(COMMON_COMPILER_FLAGS)

all: $(C_EXECUTABLES) $(CXX_EXECUTABLES)

$(CXX_EXECUTABLES): %: %.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

tokenizer.o: utility.h
tokenizer:

reader_vec.o: utility.h
reader_vec:

tf.o: utility.h
idf_dic.o: utility.h utility.hpp
w_to_vector.o: utility.h utility.hpp utility_vector.hpp utility_idf_dic.hpp
rocchio.o: utility.h utility_vector.hpp utility.hpp utility_doc_cat_list.hpp \
	utility_classifier.hpp utility_threshold_estimation.hpp rocchio.hpp
classifier.o: utility.h utility_vector.hpp utility.hpp utility_classifier.hpp
perf_measurer.o: utility.h utility_doc_cat_list.hpp

clean:
	-rm -- $(OBJECTS) > /dev/null 2>&1

mrproper: clean
	-rm -- $(C_EXECUTABLES) $(CXX_EXECUTABLES) > /dev/null 2>&1
