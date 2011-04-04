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

testing_from_step=6
file_w_vectors_name=training_set_w_vectors.bin
file_w_vectors_testing_name=testing_set_w_vectors.bin

# Default values
from_step=0
to_step=12
p_init=0
excluded_cat=unknown
use_stop_list=0
validation_testset_percentage=
skip_step_6=0
crossval_rseed=1
# End of default values

while getopts hX:t:s:r:x:a:b:B:f:lV:DR: option; do
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
	l) use_stop_list=1;;
	f) file_stop_list=$OPTARG;;
	V) validation_testset_percentage=$OPTARG;;
	D) skip_step_6=1;;
	R) crossval_rseed=$OPTARG;;
	h|?) cat >&2 <<EOF
Usage: $prog_name
       -B [INITIAL_VALUE_OF_P=$p_init]
       -t TRAINING_DIR
       -s TESTING_DIR
       -r TEMP_DIR
       -x EXEC_DIR
       -X [EXCLUDED_CATEGORY=unknown]
       -l [ENABLE_STOP_LIST=no]
       -f [STOP_LIST_FILE=EXEC_DIR/english.stop]
       -V [VALIDATION_TESTING_SET_PERCENTAGE=]
       -D [SKIP_STEP_6=no]
       -R [RANDOM_SEED_FOR_CROSS_VALIDATION_SPLIT=$crossval_rseed]
       -a [EXECUTE_FROM_STEP_A=$from_step]
       -b [EXECUTE_TO_STEP_B=$to_step]

Do not use any path name having shell special characters or whitespaces.
The name of excluded category must not contain any shell special character or
    whitespace.

For an arbitrary selection of random seed, specify -1 to -R.

To build training and testing sets according to cross validation technique, specify -V, give the percentage of documents that should go to the testing set as the argument, and run Step 2. The percentage is a real number between 0 and 100, inclusive. This option works by replacing both Step 2 and Step $((testing_from_step + 1)) with a single step that builds DOC and DOC_CAT files for both training and testing phases following cross validation approach. Step $((testing_from_step + 1)) will automatically be run unless -D is specified.

Available steps:
    0.  Constructing temporary directory structure
    [TRAINING PHASE]
    1. Tokenization, filtering stopped words if desired, and TF calculation
    2. DOC and DOC_CAT files generation, or if -V is specified, DOC and DOC_CAT
       files generation for both the training and testing phases
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
    $((testing_from_step + 0)). Tokenization, filtering stopped words if desired, and TF calculation
    $((testing_from_step + 1)). DOC and DOC_CAT files generation, or if -V is specified,
       this step is skipped
    $((testing_from_step + 2)). w vectors generation
    $((testing_from_step + 3)). OVA (One-vs-All) classification of test set
    - Dot product between a document and all binary classifiers are performed.
      Then, the document is assigned the categories associated with the
      binary classifiers that accept the document.
    $((testing_from_step + 4)). Measuring performances of classifiers
    [EXTRA STEPS (TRAINING PHASE must be done first; TESTING_PHASE is unneeded)]
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
if [ $use_stop_list -eq 1 ]; then
    stop_list=$exec_dir/stop_list
    if [ ! -x $stop_list ]; then
	echo "stop list does not exist or is not executable" >&2
	exit 1
    fi
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
cross_validation_splitter=$exec_dir/crossval_splitter
if [ \! -x $perf_measurer ]; then
    echo "crossval_splitter does not exist or is not executable" >&2
    exit 1
fi
# End of executable files

tmp_training_dir=$tmp_dir/training
tmp_testing_dir=$tmp_dir/testing
crossval_dir=$tmp_dir/crossval

if [ $use_stop_list -eq 1 -a -z "$file_stop_list" ]; then
    file_stop_list=$exec_dir/english.stop
fi
if [ $crossval_rseed -eq -1 ]; then
    crossval_rseed=$RANDOM
fi

# Intermediate files of training phase
file_idf_dic=$tmp_dir/idf_dic.bin
file_w_vectors_training=$tmp_dir/$file_w_vectors_name
file_W_vectors=$tmp_dir/W_vectors.bin
file_doc_cat_training=$tmp_dir/training_set_doc_cat.txt
file_doc_training=$tmp_dir/training_set_doc.txt
# End of intermediate files of training phase

# Intermediate files of testing phase
file_w_vectors_testing=$tmp_dir/$file_w_vectors_testing_name
file_doc_cat_testing=$tmp_dir/testing_set_doc_cat.txt
file_doc_cat_perf_measure=$tmp_dir/testing_set_perf_measure_doc_cat.txt
file_doc_testing=$tmp_dir/testing_set_doc.txt
file_classification=$tmp_dir/classification.txt
file_perf_measure=$tmp_dir/perf_measure.txt
# End of intermediate files of testing

# Intermediate files of cross validation setup
file_doc_cat_crossval_all=$tmp_dir/crossval_all_doc_cat.txt
# End of cross validation setup

# Intermediate files of the extra steps
file_classification_training=$tmp_dir/classification_on_training_set.txt
file_perf_measure_training=$tmp_dir/perf_measure_on_training_set.txt
file_doc_cat_perf_measure_training=$tmp_dir/training_set_perf_measure_doc_cat.txt
# End of intermediate files of the extra steps

# Start of common functions

# $1 is the corpus training/testing directory
# $2 is the resulting directory to store processed files from directory in $1
function create_dir_struct {
    repo_dir=$2/repo

    mkdir -p $repo_dir

    # This will kill all docs assigned to multiple categories
    curr_dir="$(pwd)"
    for dir in $1/*; do
	cd "$dir"
	find . -type f -print0 | cpio -up0 --quiet $repo_dir
    done
    cd "$curr_dir"
}

# $1 is the directory produced by function create_dir_struct
function tokenization_and_tf_calculation {
    repo_dir=$1/repo

    if [ $use_stop_list -eq 1 ]; then

	# As stated in the BASH manual, the use of a pipe will make the shell
	# waits for all children to complete.

	# 40.731s at CPU usage of 100.00% with user/sys ratio of .541
	# Try to just use a simple pipe? Or, avoid the mv first?
	(for file in `ls $repo_dir`; do
 	    ($tokenizer -o $repo_dir/$file.tok $repo_dir/$file
	     mv $repo_dir/$file{.tok,}
	     echo $repo_dir/$file) &
	 done | $stop_list -D $file_stop_list) \
	| xargs -P 0 -I'{}' $tf -o $1/'{}' $repo_dir/'{}'
    else
	for file in `ls $repo_dir`; do
	    ($tokenizer $repo_dir/$file \
		| grep -v '^[0-9a-z]$' | $tf -o $1/$file) &
	done
    fi

    wait
}

function get_doc_cat {
    # Just swap the location of dir name and file name
    sed -e 's%\(.*\)/\(.*\)%\2 \1%'
}

# $1 is the corpus training/testing directory
# $2 is the directory produced by function create_dir_struct
# $3 is the resulting DOC file containing the paths to each TF file of each 
#       unique doc across all cats
# $4 is the resulting DOC_CAT file containing docs across all cats other than
#       the excluded cat
function DOC_and_DOC_CAT_files_generation {
    find $2/ -maxdepth 1 -type f > $3
    find $1/ -mindepth 2 -type f -printf '%P\n' | get_doc_cat \
	| grep -v $excluded_cat\$ > $4
}

# $1 is the corpus training/testing directory
# $2 is the resulting DOC_CAT file containing docs across all cats.
function perf_measure_DOC_CAT_files_generation {
    find $1/ -mindepth 2 -type f -printf '%P\n' | get_doc_cat > $2
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
    time (tokenization_and_tf_calculation $tmp_training_dir) \
	|| exit 1
}

function step_2 {
    if [ -n "$validation_testset_percentage" ]; then
	step_6
	echo -n "2. [CROSS VALIDATION SETUP] Generating files... [rseed=$crossval_rseed]"
	time (
	    if [ -d $crossval_dir ]; then
		rm -r $crossval_dir
	    fi
	    mkdir $crossval_dir

# 1. Create a uniquely sorted DOC_CAT file from both training and testing sets.
	    perf_measure_DOC_CAT_files_generation $training_dir \
		$file_doc_cat_perf_measure_training
	    perf_measure_DOC_CAT_files_generation $testing_dir \
		$file_doc_cat_perf_measure
	    cat $file_doc_cat_perf_measure_training $file_doc_cat_perf_measure \
		| sort -u > $file_doc_cat_crossval_all

# 2. Copy documents from both training and testing sets into one directory.
#    This kills all duplicated docs.
	    curr_dir="$(pwd)"
	    cd $tmp_training_dir
	    find . -maxdepth 1 -type f -print0 | cpio -0up --quiet $crossval_dir
	    cd $tmp_testing_dir
	    find . -maxdepth 1 -type f -print0 | cpio -0up --quiet $crossval_dir
	    cd "$curr_dir"

# 3. Iterate the copied files at each of which do a pseudorandom decision whether to put the document in testing DOC file or training DOC file. When a document is put in testing DOC file, grep the DOC_CAT file previously created for the document name and store the result in testing DOC_CAT file. This is also the case when putting a document in training DOC file.
	    find $crossval_dir/ -type f | $cross_validation_splitter \
		-D $file_doc_cat_crossval_all \
		-P $validation_testset_percentage \
		-X $excluded_cat \
		-S $crossval_rseed \
		-1 $file_doc_training \
		-2 $file_doc_cat_training \
		-3 $file_doc_cat_perf_measure_training \
		-4 $file_doc_testing \
		-5 $file_doc_cat_testing \
		-6 $file_doc_cat_perf_measure
	)
    else
	echo -n "2. [TRAINING] DOC and DOC_CAT files generation..."
	time (DOC_and_DOC_CAT_files_generation $training_dir $tmp_training_dir \
	    $file_doc_training $file_doc_cat_training \
	    && perf_measure_DOC_CAT_files_generation $training_dir \
	    $file_doc_cat_perf_measure_training) \
	    || exit 1
    fi
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
    time ($rocchio -D $file_doc_cat_training -B $p_init \
	-U $file_w_vectors_training \
	-o $file_W_vectors $file_w_vectors_training) \
	|| exit 1
}

function step_6 {
    if [ $skip_step_6 -ne 1 ]; then
	echo -n "6. [TESTING] Tokenization and TF calculation..."
	time (tokenization_and_tf_calculation $tmp_testing_dir) \
	    || exit 1
    fi
}

function step_7 {
    echo -n "7. [TESTING] DOC and DOC_CAT files generation..."
    if [ -n "$validation_testset_percentage" ]; then
	echo " [Using cross-validation DOC and DOC_CAT files]"
    else
	time (DOC_and_DOC_CAT_files_generation $testing_dir $tmp_testing_dir \
	    $file_doc_testing $file_doc_cat_testing \
	    && perf_measure_DOC_CAT_files_generation $testing_dir \
	    $file_doc_cat_perf_measure) \
	    || exit 1
    fi
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

    if [ -n "$validation_testset_percentage" -a $i -eq 6 ]; then
	continue
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
