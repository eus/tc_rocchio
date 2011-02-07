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

# CAUTION: `wait' has to be performed so that the timing information is
# accurate. Otherwise, driver.sh has quitted before all background children
# finish processing. For example:
# time (for num in 1 2 3; do sleep 1 & done) is wrong.
# time (for num in 1 2 3; do sleep 1 & done; wait) is correct.

TIMEFORMAT=' \[%3Rs at CPU usage of %P%% with user/sys ratio of `echo "scale=3; %3U/%3S;" | bc 2>/dev/null`\]'

prog_name=driver.sh

testing_from_step=6;

# Default values
from_step=0;
to_step=12;
p_init=0;
p_inc=1;
p_max=30;
ES_count=20;
ES_percentage=30;
ES_rseed=1;
excluded_cat=unknown
# End of default values

while getopts hX:t:s:r:x:a:b:B:I:M:E:P:S: option; do
    case $option in
	X) excluded_cat=$OPTARG;;
	t) training_dir=$OPTARG;;
	s) testing_dir=$OPTARG;;
	r) tmp_dir=$OPTARG;;
	x) exec_dir=$OPTARG;;
	p) f_selection_rate=$OPTARG;;
	a) from_step=$OPTARG;;
	b) to_step=$OPTARG;;
	B) p_init=$OPTARG;;
	I) p_inc=$OPTARG;;
	M) p_max=$OPTARG;;
	E) ES_count=$OPTARG;;
	P) ES_percentage=$OPTARG;;
	S) ES_rseed=$OPTARG;;
	h|?) cat >&2 <<EOF
Usage: $prog_name
       -B [INITIAL_VALUE_OF_P=$p_init]
       -I [INCREMENT_OF_P=$p_inc]
       -M [MAXIMUM_VALUE_OF_P=$p_max]
       -E [NUMBER_OF_ESTIMATION_SETS=$ES_count]
       -P [PERCENTAGE_OF_DOCS_IN_ES=$ES_percentage]
       -S [RANDOM_SEED_FOR_ES_GENERATION=$ES_rseed]
       -t TRAINING_DIR
       -s TESTING_DIR
       -r TEMP_DIR
       -x EXEC_DIR
       -X [EXCLUDED_CATEGORY=unknown]
       -a [EXECUTE_FROM_STEP_A=$from_step]
       -b [EXECUTE_TO_STEP_B=$to_step]

Do not use any path name having shell special characters or whitespaces.
The name of excluded category must not contain any shell special character or
    whitespace.

Available steps:
    0.  Constructing temporary directory structure
    [TRAINING PHASE]
    1. Tokenization and TF calculation
    2. DOC and DOC_CAT files generation
    3. IDF calculation and DIC building
    4. w vectors generation
    5. Parametrized Rocchio Classifiers (PRCs) generation
    - The classifier of a category C is binary and consists of one W vector
      and one threshold in which any w vector whose dot product with W vector
      is less than the threshold is classified into ~C (not C). Otherwise, the
      w vector is classified into C.
    - While the W vector is obtained using parametrized Rocchio formula, the
      threshold is obtained through estimation using interpolated BEP.
    [TESTING PHASE]
    $((testing_from_step + 0)). Tokenization and TF calculation
    $((testing_from_step + 1)). DOC and DOC_CAT files generation
    $((testing_from_step + 2)). w vectors generation
    $((testing_from_step + 3)). OVA (One-vs-All) classification of test set
    - Dot product between a document and all binary classifiers are performed.
      Then, the document is assigned the categories associated with the
      binary classifiers that accept the document.
    $((testing_from_step + 4)). Measuring performances of classifiers
    [EXTRA STEPS]
    $((testing_from_step + 5)). OVA classification of the training set itself
    $((testing_from_step + 6)). Measuring performances of classifiers on the training set itself
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
if [ x$testing_dir == x ]; then
    echo "Testing directory must be specified (-h for help)" >&2
    exit 1
fi
if [ \! -d $testing_dir ]; then
    echo "Testing directory does not exist" >&2
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

# Executable files
tokenizer=$exec_dir/tokenizer
if [ \! -x $tokenizer ]; then
    echo "tokenizer does not exist or is not executable" >&2
    exit 1
fi
tf=$exec_dir/tf
if [ \! -x $tf ]; then
    echo "tf does not exist or is not executable" >&2
    exit 1
fi
idf_dic=$exec_dir/idf_dic
if [ \! -x $idf_dic ]; then
    echo "idf_dic does not exist or is not executable" >&2
    exit 1
fi
w_to_vector=$exec_dir/w_to_vector
if [ \! -x $w_to_vector ]; then
    echo "w_to_vector does not exist or is not executable" >&2
    exit 1
fi
rocchio=$exec_dir/rocchio
if [ \! -x $rocchio ]; then
    echo "rocchio does not exist or is not executable" >&2
    exit 1
fi
classifier=$exec_dir/classifier
if [ \! -x $classifier ]; then
    echo "classifier does not exist or is not executable" >&2
    exit 1
fi
perf_measurer=$exec_dir/perf_measurer
if [ \! -x $perf_measurer ]; then
    echo "perf_measurer does not exist or is not executable" >&2
    exit 1
fi
# End of executable files

tmp_training_dir=$tmp_dir/training
tmp_testing_dir=$tmp_dir/testing

# Intermediate files of training phase
file_idf_dic=$tmp_dir/idf_dic.bin
file_w_vectors_training=$tmp_dir/training_set_w_vectors.bin
file_W_vectors=$tmp_dir/W_vectors.bin
file_doc_cat_training=$tmp_dir/training_set_doc_cat.txt
file_doc_training=$tmp_dir/training_set_doc.txt
# End of intermediate files of training phase

# Intermediate files of testing phase
file_w_vectors_testing=$tmp_dir/testing_set_w_vectors.bin
file_doc_cat_testing=$tmp_dir/testing_set_doc_cat.txt
file_doc_cat_perf_measure=$tmp_dir/testing_set_perf_measure_doc_cat.txt
file_doc_testing=$tmp_dir/testing_set_doc.txt
file_classification=$tmp_dir/classification.txt
file_perf_measure=$tmp_dir/perf_measure.txt
# End of intermediate files of testing

# Intermediate files of the extra steps
file_classification_training=$tmp_dir/classification_on_training_set.txt
file_perf_measure_training=$tmp_dir/perf_measure_on_training_set.txt
file_doc_cat_perf_measure_training=$tmp_dir/training_set_perf_measure_doc_cat.txt
# End of intermediate files of the extra steps

# Start of common functions

# $1 is the template directory
# $2 is the construction directory
function create_dir_struct {
    find $1 -mindepth 1 -maxdepth 1 -type d \
    	| sed -e 's%.*/\(.*\)%'$2'/\1%' \
    	| xargs -x mkdir -p
}

# $1 is the document directory
# $2 is the result directory
function tokenization_and_tf_calculation {
    for directory in `ls $2`; do
	cat=$1/$directory
	for file in `ls $cat`; do
 	    ($tokenizer $cat/$file \
		| $tf -o $2/$directory/$file) &
	done
    done; wait
}

# $1 is the leading path before the directory having the category name
function get_doc_cat {
    sed -e 's%'$1'/\(.*\)/\(.*\)%\2 \1%'
}

# $1 is the TF files directory
# $2 is the resulting DOC file
# $3 is the resulting DOC_CAT file
function DOC_and_DOC_CAT_files_generation {
    find $1 -mindepth 2 -type f \
	| tee $2 \
	| get_doc_cat $1 \
	| grep -v $excluded_cat > $3
}

# $1 is the TF files directory
# $2 is the resulting DOC_CAT file
function perf_measure_DOC_CAT_files_generation {
    find $1 -mindepth 2 -type f | get_doc_cat $1 > $2
}

# $1 is the DOC file
# $2 is the resulting w vectors file
function w_vectors_generation {
    $w_to_vector -D $file_idf_dic -o $2 $1
}

# End of common functions

function step_0 {
    echo -n "0. Constructing temporary directory structure..."
    time if [ ! -d $tmp_dir ]; then
	create_dir_struct $training_dir $tmp_training_dir \
	    && create_dir_struct $testing_dir $tmp_testing_dir
	fi \
	|| exit 1
}

function step_1 {
    echo -n "1. [TRAINING] Tokenization and TF calculation..."
    time (tokenization_and_tf_calculation $training_dir $tmp_training_dir) \
	|| exit 1
}

function step_2 {
    echo -n "2. [TRAINING] DOC and DOC_CAT files generation..."
    time (DOC_and_DOC_CAT_files_generation $tmp_training_dir \
	$file_doc_training $file_doc_cat_training \
	&& perf_measure_DOC_CAT_files_generation $tmp_training_dir \
	   $file_doc_cat_perf_measure_training) \
	|| exit 1
}

function step_3 {
    echo -n "3. [TRAINING] IDF calculation and DIC building..."
    time ($idf_dic -o $file_idf_dic $file_doc_training) \
	|| exit 1
}

function step_4 {
    echo -n "4. [TRAINING] w vectors generation..."
    time (w_vectors_generation $file_doc_training $file_w_vectors_training) \
	|| exit 1
}

function step_5 {
    echo -n "5. [TRAINING] PRCs generation..."
    time ($rocchio -D $file_doc_cat_training -B $p_init -I $p_inc -M $p_max \
	-E $ES_count -P $ES_percentage -S $ES_rseed -o $file_W_vectors \
	$file_w_vectors_training) \
	|| exit 1
}

function step_6 {
    echo -n "6. [TESTING] Tokenization and TF calculation..."
    time (tokenization_and_tf_calculation $testing_dir $tmp_testing_dir) \
	|| exit 1
}

function step_7 {
    echo -n "7. [TESTING] DOC and DOC_CAT files generation..."
    time (DOC_and_DOC_CAT_files_generation $tmp_testing_dir \
	$file_doc_testing $file_doc_cat_testing \
	&& perf_measure_DOC_CAT_files_generation $tmp_testing_dir \
	   $file_doc_cat_perf_measure) \
	|| exit 1
}

function step_8 {
    echo -n "8. [TESTING] w vectors generation..."
    time (w_vectors_generation $file_doc_testing $file_w_vectors_testing) \
	|| exit 1
}

function step_9 {
    echo -n "9. [TESTING] OVA classification of test set..."
    time ($classifier -D $file_W_vectors -o $file_classification \
	$file_w_vectors_testing) \
	|| exit 1
}

function step_10 {
    echo -n "10. [TESTING] Measuring performances of classifiers..."
    time ($perf_measurer -D $file_doc_cat_perf_measure -X $excluded_cat \
	-o $file_perf_measure $file_classification) \
	|| exit 1
}

function step_11 {
    echo -n "11. [EXTRA STEP] OVA classification of training set..."
    time ($classifier -D $file_W_vectors -o $file_classification_training \
	$file_w_vectors_training) \
	|| exit 1
}

function step_12 {
    echo -n "12. [EXTRA STEP] Measuring performances of classifiers..."
    time ($perf_measurer -D $file_doc_cat_perf_measure_training \
	-X $excluded_cat -o $file_perf_measure_training \
	$file_classification_training) \
	|| exit 1
}

# The jujitsu of eval and sed is needed to pretty print the timing
# information while preserving any error message. I assume that there is no
# shell metacharacters in the timing line that needs to be escaped by sed to
# avoid more complicated jujitsu of sed
not_timing_line='/\\\[.*at CPU usage.*\\\]$/!'
    timing_line='/\\\[.*at CPU usage.*\\\]$/' # same as before but no `!'
for ((i = $from_step; i <= $to_step; i++)); do
    eval `step_$i \
	2>&1 \
	| sed \
	-e "$not_timing_line s%'%'\\\\''%g" \
	-e "$not_timing_line s%.*%echo '&';%" \
	-e "$timing_line"' s%.*%echo &%'` \
	| sed \
	-e 's% with user/sys ratio of ]% with user/sys ratio of infinity]%'
done

exit 0
