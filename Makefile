.PHONY = all clean mrproper

EXECUTABLES := tokenizer tf idf reader_2col streaming_tokenizer w
OBJECTS := tokenizer.o tf.o idf.o reader_2col.o streaming_tokenizer.o w.o

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

tf.o: utility.h
tf: tf.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

idf.o: utility.h
idf: idf.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

w.o: utility.h
w: w.o
	$(CXX) -o $@ $(LDFLAGS) $+ $(LOADLIBES) $(LDLIBS)

clean:
	-rm -- $(OBJECTS) > /dev/null 2>&1

mrproper: clean
	-rm -- $(EXECUTABLES) > /dev/null 2>&1
