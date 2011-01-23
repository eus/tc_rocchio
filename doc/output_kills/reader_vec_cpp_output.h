#ifndef READER_VEC_CPP_OUTPUT
#define READER_VEC_CPP_OUTPUT
#include <iostream>
#include <fstream>
#include <cstdio>
#include "../../utility.h"

using namespace std;

static ofstream outfile;

static inline void unset_cpp_output()
{
  if (outfile.is_open()) {
    outfile.close();
  }
}

static inline void set_cpp_output()
{
  if (out_stream != stdout) {
    if (fclose(out_stream) != 0) {
      fatal_syserror("Cannot close output %s", out_stream_name);
    }
    out_stream = stdout;
  }

  outfile.open(out_stream_name, ios::trunc);
  outfile.setf(ios::fixed, ios::floatfield);
  outfile.precision(6);
}

#endif /* READER_VEC_CPP_OUTPUT */
