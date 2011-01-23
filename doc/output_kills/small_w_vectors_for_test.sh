#!/bin/bash

(make mrproper && make TESTING=yes) || exit 1

exec_count=`ls *.cpp | wc -l`
for ((i = 1; i < $exec_count; i++)); do
    valgrind -q --leak-check=full ./reader_vec_$i -o test_case.txt small_w_vectors_for_test.bin > valgrind.txt 2>&1
    if [ `stat -c %s valgrind.txt` -ne 0 ]; then
	echo "Valgrind test fails (see valgrind.txt)"
	exit 1
    else
	rm valgrind.txt
    fi
    diff test_case.txt small_w_vectors_for_test.test > /dev/null
    if [ $? == 1 ]; then
	echo "reader_vec_$i fails (see test_case.txt)"
	exit 1
    fi
    rm test_case.txt
done

echo "Everything is okay"

exit 0