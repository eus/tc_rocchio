#!/bin/bash

#############################################################################
# Copyright (C) 2011  Tadeus Prastowo (eus@member.fsf.org)                  #
#                                                                           #
# This program is free software: you can redistribute it and/or modify      #
# it under the terms of the GNU General Public License as published by      #
# the Free Software Foundation, either version 3 of the License, or         #
# (at your option) any later version.                                       #
#                                                                           #
# This program is distributed in the hope that it will be useful,           #
# but WITHOUT ANY WARRANTY; without even the implied warranty of            #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
# GNU General Public License for more details.                              #
#                                                                           #
# You should have received a copy of the GNU General Public License         #
# along with this program.  If not, see <http://www.gnu.org/licenses/>.     #
#############################################################################

TIMEFORMAT=' \[%3Rs at CPU usage of %P%% with user/sys ratio of `echo "scale=3; %3U/%3S;" | bc 2>/dev/null`\]'

prog_name=roi_driver.sh

# Default values
from_step=0
to_step=7
ES_rseed=1
# End of default values

while getopts ht:r:x:a:b:S: option; do
    case $option in
	t) training_dir=$OPTARG;;
	r) tmp_dir=$OPTARG;;
	x) exec_dir=$OPTARG;;
	a) from_step=$OPTARG;;
	b) to_step=$OPTARG;;
	S) ES_rseed=$OPTARG;;
	h|?) cat >&2 <<EOF
Usage: $prog_name
       -S [RANDOM_SEED_FOR_ES_GENERATION=$ES_rseed]
       -t TRAINING_DIR
       -r TEMP_DIR
       -x EXEC_DIR
       -a [EXECUTE_FROM_STEP_A=$from_step]
       -b [EXECUTE_TO_STEP_B=$to_step]

Do not use any path name having shell special characters or whitespaces.

Available steps:
    0.  Constructing temporary directory structure
    1. Tokenizing
    2. TF generation
    3. Putting all training docs into splitClasses/...
    4. IDF_nf building...
    5. Generating binary classifiers and doing classifications...
    6. Put all training docs into classes/...
    7. Measuring performance...
EOF
	    exit 0;;
    esac
done

if [ x$training_dir == x ]; then
    echo "Training directory must be specified (-h for help)" >&2
    exit 1
fi
if [ \! -d $training_dir ]; then
    echo "Training directory does not exist" >&2
    exit 1
fi
if [ x$tmp_dir == x ]; then
    echo "Temporary directory must be specified (-h for help)" >&2
    exit 1
fi
if [ x$exec_dir == x ]; then
    echo "Directory containing executables must be specified (-h for help)" >&2
    exit 1
fi
if [ \! -d $exec_dir ]; then
    echo "Directory containing executables does not exist" >&2
    exit 1
fi

data_dir=$tmp_dir/clusteringCategories
CKB_tmp_dir=$tmp_dir/temp
tokenizing_dir=$tmp_dir/tokenizing

CKB_dir=$tmp_dir/CKB
CKB_split_dir=$CKB_dir/splitClasses
CKB_cce_dir=$CKB_dir/cce
CKB_test_dir=$CKB_dir/testdoc
CKB_store_dir=$CKB_dir/store
CKB_class_dir=$CKB_dir/classes

# Executable files
tokenizer=$exec_dir/tokenizer
if [ \! -x $tokenizer ]; then
    echo "tokenizer does not exist or is not executable" >&2
    exit 1
fi
tcf=$exec_dir/TCF
if [ \! -x $tcf ]; then
    echo "TCF does not exist or is not executable" >&2
    exit 1
fi
# End of executable files

file_BEP=$tmp_dir/BEP

export gamma=1

function step_0 {
    echo -n "0. Constructing temporary directory structure..."
    time (rm -rf $CKB_dir $data_dir $CKB_tmp_dir $tokenizing_dir \
	&& mkdir -p $CKB_tmp_dir $tokenizing_dir $data_dir \
	   $CKB_split_dir $CKB_cce_dir $CKB_test_dir $CKB_store_dir \
	   $CKB_class_dir \
	&& ln -s $exec_dir $tmp_dir/bin) \
	|| exit 1
}

function step_1 {
    echo -n "1. Tokenizing..."
    time (cd $tokenizing_dir \
    	&& (find $training_dir -mindepth 1 -maxdepth 1 -type d \
    	| sed -e 's%.*/\(.*\)%\1%' \
    	| xargs -x mkdir) \
	&& (for directory in `ls`; do
	      for file in `ls $training_dir/$directory`; do
		($tokenizer $training_dir/$directory/$file \
		    | sed -e 's%.*%'$file'\t&\t1\t1%' \
		    > $directory/$file.tok) &
	      done
	    done; wait) \
	&& (for directory in `ls`; do
	      cat $directory/*.tok > $data_dir/$directory.le &
	    done; wait)) \
		|| exit 1
}

function step_2 {
    echo -n "2. TF generation..."
    time ($tcf UNI -RC$data_dir > /dev/null 2>&1) \
	|| exit 1
}

function step_3 {
    echo -n "3. Putting all training docs into splitClasses/..."
    time ($tcf CCE -SP0 -SE$ES_rseed > /dev/null 2>&1) \
	|| exit 1
}

function step_4 {
    echo -n "4. IDF_nf building..."
    time ($tcf GCE -DF0 > /dev/null 2>&1) \
	|| exit 1
}

function step_5 {
    echo -n "5. Generating binary classifiers and doing classifications..."
    time ($tcf DIC -GA1 > /dev/null 2>&1) \
	|| exit 1
}

function step_6 {
    echo -n "6. Put all training docs into classes/..."
    time ($tcf CCE -SP100 -SE$ES_rseed > /dev/null 2>&1) \
	|| exit 1
}

function step_7 {
    echo -n "7. Measuring performance..."
    time ($tcf CLA -BP > $file_BEP) \
	|| exit 1
}

# The jujitsu of eval and sed is needed to pretty print the timing
# information while preserving any error message. I assume that there is no
# shell metacharacters in the timing line that needs to be escaped by sed to
# avoid more complicated jujitsu of sed
not_timing_line='/\\\[.*at CPU usage.*\\\]$/!'
    timing_line='/\\\[.*at CPU usage.*\\\]$/' # same as before but no `!'
for ((i = $from_step; i <= $to_step; i++)); do
    if [ -d $tmp_dir ]; then
	cd $tmp_dir
    fi
    eval `step_$i \
	2>&1 \
	| sed \
	-e "$not_timing_line s%'%'\\\\''%g" \
	-e "$not_timing_line s%.*%echo -e '&';%" \
	-e "$timing_line"' s%.*%echo -e &;%'` \
	| sed \
	-e 's% with user/sys ratio of ]% with user/sys ratio of infinity]%'
done

exit 0
