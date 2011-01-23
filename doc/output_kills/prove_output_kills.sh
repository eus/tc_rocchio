#!/bin/bash

(make mrproper && make) || exit 1

TIMEFORMAT=' \[%3Rs at CPU usage of %P%% with user/sys ratio of `echo "scale=3; %3U/%3S;" | bc 2>/dev/null`\]'

# The jujitsu of eval and sed is needed to pretty print the timing
# information while preserving any error message. I assume that there is no
# shell metacharacters in the timing line that needs to be escaped by sed to
# avoid more complicated jujitsu of sed
not_timing_line='/\\\[.*at CPU usage.*\\\]$/!'
    timing_line='/\\\[.*at CPU usage.*\\\]$/' # just throwing `!' away

exec_count=`ls *.cpp | wc -l`
for ((i = 1; i <= $exec_count; i++)); do
    echo -n "reader_vec_$i: "
    eval `(time ./reader_vec_$i -o /dev/null w_vectors.bin) \
	2>&1 \
	| sed \
	-e "$not_timing_line s%'%'\\\\''%g" \
	-e "$not_timing_line s%.*%echo '&';%" \
	-e "$timing_line"' s%.*%echo &%'` \
	| sed \
	-e 's% with user/sys ratio of ]% of infinity]%'
done

exit 0
