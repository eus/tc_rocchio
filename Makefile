.PHONY = all clean mrproper

EXECUTABLES := tokenizer tf idf_dic reader_2col streaming_tokenizer \
	w_to_vector reader_vec
OBJECTS := tokenizer.o tf.o idf_dic.o reader_2col.o streaming_tokenizer.o \
	w_to_vector.o reader_vec.o

CPPFLAGS := -DBUFFER_SIZE=4096
CFLAGS := -Wall -O3
CXXFLAGS := -std=c++0x -Wall -O3

all: $(EXECUTABLES)

tokenizer.o: utility.h
tokenizer:

streaming_tokenizer.o: utility.h
streaming_tokenizer:

reader_2col.o: utility.h
reader_2col:

reader_vec.o: utility.h
reader_vec:

tf.o: utility.h
tf: tf.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

idf_dic.o: utility.h
idf_dic: idf_dic.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

w_to_vector.o: utility.h
w_to_vector: w_to_vector.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

clean:
	-rm -- $(OBJECTS) > /dev/null 2>&1

mrproper: clean
	-rm -- $(EXECUTABLES) > /dev/null 2>&1
