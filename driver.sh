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

TIMEFORMAT=' [%3Rs at CPU usage of %P%%]'

prog_name=driver.sh

while getopts ht:r:x: option; do
    case $option in
	t) training_dir=$OPTARG;;
	r) result_dir=$OPTARG;;
	x) exec_dir=$OPTARG;;
	p) f_selection_rate=$OPTARG;;
	h|?) cat >&2 <<EOF
Usage: $prog_name
       -p [FEATURE_SELECTION_RATE=1]
       -t TRAINING_DIR
       -r TEMP_DIR
       -x EXEC_DIR

Do not use any path name having shell special characters or whitespaces.
EOF
    esac
done

if [ x$training_dir == x ]; then
    echo "Training directory must be specified (-h for help)" >&2
    exit 1
fi
if [ x$result_dir == x ]; then
    echo "Result directory must be specified (-h for help)" >&2
    exit 1
fi
if [ x$exec_dir == x ]; then
    echo "Directory containing executables must be specified (-h for help)" >&2
    exit 1
fi
if [ x$f_selection_rate == x ]; then
    f_selection_rate=1
fi

tokenizer=$exec_dir/tokenizer
tf=$exec_dir/tf
idf_dic=$exec_dir/idf_dic
rocchio=$exec_dir/rocchio
classifier=$exec_dir/classifier
file_idf_dic=$result_dir/idf_dic.bin
file_w_vectors=$result_dir/w_vectors.bin
file_W_vectors=$result_dir/W_vectors.bin
file_doc_cat=$result_dir/doc_cat.txt
file_classification=$result_dir/classification.txt

echo -n "0. Constructing temporary directory structure..."
time if [ ! -d $result_dir ]; then
sleep 1
    mkdir $result_dir \
    	&& cd $result_dir \
    	&& find $training_dir -mindepth 1 -maxdepth 1 -type d \
    	| sed -e 's%.*/\(.*\)%\1%' \
    	| xargs mkdir
fi

echo -n "1. Tokenization and TF calculation..."
time ( for directory in `ls $result_dir`; do
    cat=$training_dir/$directory
    for file in `ls $cat`; do
 	( $tokenizer $cat/$file \
	    | $tf -o $result_dir/$directory/$file.tf) &
    done
done; wait )

echo -n "2. IDF calculation and DIC building..."
time (
doc_count=`find $result_dir -type f | wc -l`
find $result_dir -type f -iname '*.tf' \
    | xargs cat \
    | $idf_dic -M $doc_count -o $file_idf_dic )

echo -n "3. w vectors and DOC_CAT generations..."
time ( cd $result_dir \
    && ls */* \
    | tee $file_doc_cat \
    | xargs $exec_dir/w_to_vector -D $file_idf_dic -o $file_w_vectors )

echo -n "4. DOC_CAT completion and W vectors generation..."
time (sed -i -e 's%\(.*\)/.*%& \1%' $file_doc_cat \
    && $rocchio -D $file_doc_cat -p $f_selection_rate -o $file_W_vectors \
    $file_w_vectors )

echo -n "5. Classifying the training set itself..."
time (
    cat_count=`cut -d ' ' -f 2 $file_doc_cat | sort -u | wc -l`
    $classifier -M $cat_count -D $file_W_vectors -o $file_classification \
	$file_w_vectors )

exit 0
