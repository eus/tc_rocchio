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

prog_name=run_experiment.sh

# Must be changed to reproduce a particular result
rseeds_file=
# End of parameter that must be change to produce a particular result

# Modifiable without changing reproducibility
thread_count=
reuters_corpus_dir=
tc_rocchio_exec_dir=
result_base_dir=
# End of modifiable parameters that do not affect reproducibility

while getopts hC:r:x:J:R: option; do
    case $option in
	C) reuters_corpus_dir=$OPTARG;;
	r) result_base_dir=$OPTARG;;
	x) tc_rocchio_exec_dir=$OPTARG;;
	J) thread_count=$OPTARG;;
	R) rseeds_file=$OPTARG;;
	h|?) cat >&2 <<EOF
Usage: $prog_name
       -C REUTERS_CORPUS_DIR
       -r RESULT_BASE_DIR
       -x TC_ROCCHIO_EXEC_DIR
       -J PARAMETER_TUNING_THREAD_COUNT
       -R [RANDOM_SEEDS_FILE_FOR_REPRODUCING_EXACT_RESULT]

Do not use any path name having shell special characters or whitespaces.
REUTERS_CORPUS_DIR is expected to contain the extraction result of
    http://disi.unitn.it/moschitti/corpora/Reuters21578-Apte-90Cat.tar.gz
TC_ROCCHIO_EXEC_DIR is expected to contain two directories: dont_follow_roi and
    follow_roi. The first directory should have executables compiled with \`make
    DONT_FOLLOW_ROI=enabled' while the second directory should have executables
    compiled with just \`make'.
If the result of a previous run would like to be reproduced, specify the option
    -R and give as its argument the path to the file
    RESULT_BASE_DIR/rseeds.output produced in the previous run.
To pause the experiment, create a file named PAUSE in RESULT_BASE_DIR.

EOF
	    exit 0;;
    esac
done

if [ x$reuters_corpus_dir == x ]; then
    echo "Reuters corpus directory must be specified (-h for help)" >&2
    exit 1
fi
if [ ! -d $reuters_corpus_dir ]; then
    echo "Reuters corpus directory does not exist" >&2
    exit 1
fi
if [ x$result_base_dir == x ]; then
    echo "Result base directory must be specified (-h for help)" >&2
    exit 1
fi
if [ ! -d $result_base_dir ]; then
    echo -n "Result base directory exists not, creating... "
    mkdir -p $result_base_dir
    echo "DONE"
fi
if [ x$tc_rocchio_exec_dir == x ]; then
    echo "TC Rocchio directory of execs must be specified (-h for help)" >&2
    exit 1
fi
if [ x$thread_count == x ]; then
    echo "Thread count must be specified (-h for help)" >&2
    exit 1    
fi

# Modifiable without changing reproducibility
reuters_training_dir=$reuters_corpus_dir/training
reuters_testing_dir=$reuters_corpus_dir/test

dont_follow_roi_exec_dir=$tc_rocchio_exec_dir/dont_follow_roi
follow_roi_exec_dir=$tc_rocchio_exec_dir/follow_roi

rseeds_out_file=$result_base_dir/rseeds.output
timing_result=$result_base_dir/timing.csv
performance_result=$result_base_dir/performance.csv

rseed_index_file=$result_base_dir/rseed_index_file.txt
tmp_timing_file=$result_base_dir/timing.txt
tmp_performance_file=$result_base_dir/perf.csv
# End of modifiable parameters that do not affect reproducibility

# Changing the following alters how the experiment is conducted
various_ES_percentages='30 40 50 60'
valset_count=20
valset_percentage=30
various_Ps='0.25 1'
reported_cats='earn acq money-fx grain crude trade interest ship wheat corn'
# End of parameters that alters how the experiment is conducted

echo 1 > $rseed_index_file
function next_rseed {
    rseed_index=`cat $rseed_index_file`
    if [ -z $rseeds_file ]; then
        echo '-1'
    elif [ $(cat $rseeds_file | wc -l) -lt $rseed_index ]; then
        echo 'WARNING: rseeds_file runs out of rseed, will continue with -1' >&2
        echo '-1'
    else
        tail -n +$rseed_index $rseeds_file | head -n 1
        echo $((rseed_index + 1)) > $rseed_index_file
    fi
}

function extract_rseeds {
    sed -n \
        -e '/^2\. \[CROSS VALIDATION SETUP\]/ s%.*rseed=\([^]]\+\).*%\1%p' \
        -e '/^5\. \[TRAINING\] PRCs / s%.*seed=\([^]]\+\).*%\1%p' \
        $1
}

function extract_timings {
    sed -e 's|\(.\+\) \[\([.0-9]\+\)s .\+ of \([.0-9]\+\)% .\+ of \(.\+\)\]|\1\t\2\t\3\t\4|' $1
}

# $1 title
# $2 start
# $3 end
function timing_statistics {
    echo -e '\t\t\t\t\t'$1'\t=SUM($B$'$2':$B$'$3')\t=G'$(($3 + 1))' / 60'
}

function global_subheader {
    echo -ne '\tMacro Average\t\tMicro Average'
    echo -e '\t\tMicro Average f1\tAverage BEP'
    echo -e '\tPrecision\tRecall\tPrecision\tRecall'
}

function global_header {
    echo 'Global Performance'
    echo -n 'nth cross-validation set'
    global_subheader
}

# $1 result directory
function get_global_performance {
    tail -n 1 $1/perf_measure.txt
}

# $1 row number
function global_f1 {
    echo -n 'Global'
    echo -e '\t=$F$'$1
}

# $1 start
# $2 end
function global_statistics {
    echo -n 'Global'
    echo -ne '\t=AVERAGE($F$'$1':$F$'$2')'
    echo -e '\t=STDEV($F$'$1':$F$'$2')'
}

function global_trailer {
    echo ''
}

function per_cat_subheader {
    echo -e '\tcat\ta\tb\tc\tPrecision\tRecall\tf1\tBEP'
}

function per_cat_header {
    echo "Performance on Category $cat"
    echo -n 'nth cross-validation set'
    per_cat_subheader
}

function get_per_cat_performance {
    grep -m 1 -- ^$1 $2/perf_measure.txt
}

# $1 cat_name
# $2 row number
function per_cat_f1 {
    echo -n ''$1
    echo -e '\t=$H$'$2
}

# $1 cat_name
# $2 start
# $3 end
function per_cat_statistics {
    echo -n ''$1
    echo -ne '\t=AVERAGE($H$'$2':$H$'$3')'
    echo -e '\t=STDEV($H$'$2':$H$'$3')'
}

function per_cat_trailer {
    echo ''
}

function merge_in_statistics {
    sed -e 's%.*%\t\t\t\t\t\t\t\t\t\t&%'
}

function pause_experiment {
    if [ -f $result_base_dir/PAUSE ]; then
	echo "[PAUSED] Experiment is being paused, press ENTER to continue"
	read
	rm $result_base_dir/PAUSE
    fi
}

# $1 dont_follow_roi/follow_roi
# $2 exec_dir
function run_crossval_various_ES_percentages {
    echo "[Running $1.crossval.various_ES_percentages] `date`"
    for percentage in $various_ES_percentages; do
	title="$1.crossval.ES_percentage.$percentage"
        echo "$title" | tee -a $timing_result >> $performance_result

        final_result_dir='$result_base_dir'
        final_result_dir+='/result.$1.crossval.$i.ES_percentage.$percentage'

        for ((i=1; i <= $valset_count; i++)); do
            echo "i=$i percentage=$percentage `date`"
            mkdir -p $result_base_dir/result/BEP_history
            ln -s ../raw_data/training/ $result_base_dir/result/
            ln -s ../raw_data/testing $result_base_dir/result

            # The following rseeds must be obtained in this order
            cross_rseed=$(next_rseed)
            ES_rseed=$(next_rseed)
            # End of obtaining rseeds

	    # Construct the command string
            command="$2/driver.sh"
	    command+=" -x $2"
	    command+=" -r $result_base_dir/result"
	    command+=" -t $reuters_training_dir"
	    command+=" -s $reuters_testing_dir"
	    command+=" -a 2"
	    command+=" -J $thread_count"
	    command+=" -l"
	    command+=" -P $percentage"
	    command+=" -S $ES_rseed"
	    command+=" -R $cross_rseed"
	    command+=" -D"
	    command+=" -V $valset_percentage"
	    command+=" -H $result_base_dir/result/BEP_history/history"
	    # End of constructing the command string

	    echo "$command"
	    eval $command 2>&1 | tee $tmp_timing_file

            # Extracting timing information
            extract_rseeds $tmp_timing_file >> $rseeds_out_file
            echo "$command" >> $timing_result
            extract_timings $tmp_timing_file >> $timing_result
	    end=`cat $timing_result | wc -l`
	    start=$((end - 10 + 1))
	    timing_statistics $title $start $end >> $timing_result
            # End of extracting timing information

            rm -r $result_base_dir/result/crossval # due to being reproducible
            mv $result_base_dir/result `eval echo $final_result_dir`

	    pause_experiment
        done

        # Extract performance data
        rm -f $tmp_performance_file

        # Global performance
	global_header >> $performance_result
        for ((i=1; i <= $valset_count; i++)); do
            get_global_performance `eval echo $final_result_dir` \
                | sed -e 's%.*%'$i'\t&%' \
                >> $performance_result
        done
        end=`cat $performance_result | wc -l`
        start=$((end - valset_count + 1))
	global_statistics $start $end >> $tmp_performance_file
        global_trailer >> $performance_result

        # Per category performance
        for cat in $reported_cats; do
	    per_cat_header >> $performance_result
            for ((i=1; i <= $valset_count; i++)); do
		get_per_cat_performance $cat `eval echo $final_result_dir` \
                    | sed -e 's%.*%'$i'\t&%' \
                    >> $performance_result
            done
            end=`cat $performance_result | wc -l`
            start=$((end - valset_count + 1))
	    per_cat_statistics $cat $start $end >> $tmp_performance_file
	    per_cat_trailer >> $performance_result
        done

        # Putting in the statistics
	echo "$title" | merge_in_statistics >> $performance_result
        cat $tmp_performance_file | merge_in_statistics >> $performance_result
        # End of extracting performance data
    done
}

# $1 dont_follow_roi/follow_roi
# $2 exec_dir
function run_crossval_noES {
    echo "[Running $1.crossval.noES] `date`"
    for P in $various_Ps; do
	title="$1.crossval.P.$P"
        echo "$title" | tee -a $timing_result >> $performance_result

        final_result_dir='$result_base_dir'
        final_result_dir+='/result.$1.crossval.$i.P.$P'

        for ((i=1; i <= $valset_count; i++)); do
            echo "i=$i P=$P `date`"
            mkdir $result_base_dir/result/
            ln -s ../raw_data/training/ $result_base_dir/result/
            ln -s ../raw_data/testing $result_base_dir/result

            # The following rseeds must be obtained in this order
            cross_rseed=$(next_rseed)
            # End of obtaining rseeds

	    # Construct the command string
            command="$2/driver.sh"
	    command+=" -x $2"
	    command+=" -r $result_base_dir/result"
	    command+=" -t $reuters_training_dir"
	    command+=" -s $reuters_testing_dir"
	    command+=" -a 2"
	    command+=" -J $thread_count"
	    command+=" -l"
	    command+=" -E 0"
	    command+=" -B $P"
	    command+=" -I "$(echo $P + 2 | bc)
	    command+=" -M "$(echo $P + 1 | bc)
	    command+=" -R $cross_rseed"
	    command+=" -D"
	    command+=" -V $valset_percentage"
	    # End of constructing the command string

	    echo "$command"
	    eval $command 2>&1 | tee $tmp_timing_file

            # Extracting timing information
            extract_rseeds $tmp_timing_file >> $rseeds_out_file
            echo "$command" >> $timing_result
            extract_timings $tmp_timing_file >> $timing_result
	    end=`cat $timing_result | wc -l`
	    start=$((end - 10 + 1))
	    timing_statistics $title $start $end >> $timing_result
            # End of extracting timing information

            rm -r $result_base_dir/result/crossval # due to being reproducible
            mv $result_base_dir/result `eval echo $final_result_dir`

	    pause_experiment
        done

        # Extract performance data
        rm -f $tmp_performance_file

        # Global performance
	global_header >> $performance_result
        for ((i=1; i <= $valset_count; i++)); do
            get_global_performance `eval echo $final_result_dir` \
                | sed -e 's%.*%'$i'\t&%' \
                >> $performance_result
        done
        end=`cat $performance_result | wc -l`
        start=$((end - valset_count + 1))
	global_statistics $start $end >> $tmp_performance_file
        global_trailer >> $performance_result

        # Per category performance
        for cat in $reported_cats; do
	    per_cat_header >> $performance_result
            for ((i=1; i <= $valset_count; i++)); do
		get_per_cat_performance $cat `eval echo $final_result_dir` \
                    | sed -e 's%.*%'$i'\t&%' \
                    >> $performance_result
            done
            end=`cat $performance_result | wc -l`
            start=$((end - valset_count + 1))
	    per_cat_statistics $cat $start $end >> $tmp_performance_file
	    per_cat_trailer >> $performance_result
        done

        # Putting in the statistics
	echo "$title" | merge_in_statistics >> $performance_result
        cat $tmp_performance_file | merge_in_statistics >> $performance_result
        # End of extracting performance data
    done
}

# $1 dont_follow_roi/follow_roi
# $2 exec_dir
function run_various_ES_percentages {
    echo "[Running $1.various_ES_percentages] `date`"
    for percentage in $various_ES_percentages; do
	title="$1.ES_percentage.$percentage"
        echo "$title" | tee -a $timing_result >> $performance_result

        final_result_dir='$result_base_dir'
        final_result_dir+='/result.$1.$i.ES_percentage.$percentage'

        for ((i=1; i <= $valset_count; i++)); do
            echo "i=$i percentage=$percentage `date`"
            mkdir -p $result_base_dir/result/BEP_history
            ln -s ../raw_data/training/ $result_base_dir/result/
            ln -s ../raw_data/testing $result_base_dir/result

            # The following rseeds must be obtained in this order
            ES_rseed=$(next_rseed)
            # End of obtaining rseeds

	    # Construct the command string
            command="$2/driver.sh"
	    command+=" -x $2"
	    command+=" -r $result_base_dir/result"
	    command+=" -t $reuters_training_dir"
	    command+=" -s $reuters_testing_dir"
	    command+=" -a 2"
	    command+=" -J $thread_count"
	    command+=" -l"
	    command+=" -P $percentage"
	    command+=" -S $ES_rseed"
	    command+=" -D"
	    command+=" -H $result_base_dir/result/BEP_history/history"
	    # End of constructing the command string

	    echo "$command"
	    eval $command 2>&1 | tee $tmp_timing_file

            # Extracting timing information
            extract_rseeds $tmp_timing_file >> $rseeds_out_file
            echo "$command" >> $timing_result
            extract_timings $tmp_timing_file >> $timing_result
	    end=`cat $timing_result | wc -l`
	    start=$((end - 10 + 1))
	    timing_statistics $title $start $end >> $timing_result
            # End of extracting timing information

            mv $result_base_dir/result `eval echo $final_result_dir`

	    pause_experiment
        done

        # Extract performance data
        rm -f $tmp_performance_file

        # Global performance
	global_header >> $performance_result
        for ((i=1; i <= $valset_count; i++)); do
            get_global_performance `eval echo $final_result_dir` \
                | sed -e 's%.*%'$i'\t&%' \
                >> $performance_result
        done
        end=`cat $performance_result | wc -l`
        start=$((end - valset_count + 1))
	global_statistics $start $end >> $tmp_performance_file
        global_trailer >> $performance_result

        # Per category performance
        for cat in $reported_cats; do
	    per_cat_header >> $performance_result
            for ((i=1; i <= $valset_count; i++)); do
		get_per_cat_performance $cat `eval echo $final_result_dir` \
                    | sed -e 's%.*%'$i'\t&%' \
                    >> $performance_result
            done
            end=`cat $performance_result | wc -l`
            start=$((end - valset_count + 1))
	    per_cat_statistics $cat $start $end >> $tmp_performance_file
	    per_cat_trailer >> $performance_result
        done

        # Putting in the statistics
	echo "$title" | merge_in_statistics >> $performance_result
        cat $tmp_performance_file | merge_in_statistics >> $performance_result
        # End of extracting performance data
    done
}

# $1 dont_follow_roi/follow_roi
# $2 exec_dir
function run_noES {
    echo "[Running $1.noES] `date`"
    for P in $various_Ps; do
	title="$1.P.$P"
        echo "$title" | tee -a $timing_result >> $performance_result

        final_result_dir='$result_base_dir'
        final_result_dir+='/result.$1.P.$P'

            echo "P=$P `date`"
            mkdir $result_base_dir/result/
            ln -s ../raw_data/training/ $result_base_dir/result/
            ln -s ../raw_data/testing $result_base_dir/result

	    # Construct the command string
            command="$2/driver.sh"
	    command+=" -x $2"
	    command+=" -r $result_base_dir/result"
	    command+=" -t $reuters_training_dir"
	    command+=" -s $reuters_testing_dir"
	    command+=" -a 2"
	    command+=" -J $thread_count"
	    command+=" -l"
	    command+=" -E 0"
	    command+=" -B $P"
	    command+=" -I "$(echo $P + 2 | bc)
	    command+=" -M "$(echo $P + 1 | bc)
	    command+=" -D"
	    # End of constructing the command string

	    echo "$command"
	    eval $command 2>&1 | tee $tmp_timing_file

            # Extracting timing information
            echo "$command" >> $timing_result
            extract_timings $tmp_timing_file >> $timing_result
	    end=`cat $timing_result | wc -l`
	    start=$((end - 10 + 1))
	    timing_statistics $title $start $end >> $timing_result
            # End of extracting timing information

            mv $result_base_dir/result `eval echo $final_result_dir`

	    pause_experiment

        # Extract performance data
        rm -f $tmp_performance_file

        # Global performance
	global_subheader >> $performance_result
        get_global_performance `eval echo $final_result_dir` \
            | sed -e 's%.*%\t&%' \
	    >> $performance_result
	global_f1 `cat $performance_result | wc -l` >> $tmp_performance_file
        global_trailer >> $performance_result

        # Per category performance
	per_cat_subheader >> $performance_result
        for cat in $reported_cats; do
	    get_per_cat_performance $cat `eval echo $final_result_dir` \
                | sed -e 's%.*%\t&%' \
                >> $performance_result
	    per_cat_f1 $cat `cat $performance_result | wc -l` \
		>> $tmp_performance_file
        done
	per_cat_trailer >> $performance_result

        # Putting in the statistics
	echo "$title" | merge_in_statistics >> $performance_result
        cat $tmp_performance_file | merge_in_statistics >> $performance_result
        # End of extracting performance data
    done
}

# Begin experiment

# Raw material preparation (Step 0, Step 1 and Step 6 must be the same for both
# dont_follow_roi and follow_roi)
echo "[Preparing raw material] `date`"
echo 'Tokenization and TF generation' >> $timing_result
command="$dont_follow_roi_exec_dir/driver.sh"
command+=" -t $reuters_training_dir"
command+=" -s $reuters_testing_dir"
command+=" -x $dont_follow_roi_exec_dir"
command+=" -r $result_base_dir/raw_data"
command+=" -l"
command+=" -a 0"
command+=" -b 1"
echo "$command" >> $timing_result
echo "$command"
eval $command 2>&1 | tee $tmp_timing_file
extract_timings $tmp_timing_file >> $timing_result
echo "$command" >> $timing_result
command="$dont_follow_roi_exec_dir/driver.sh"
command+=" -t $reuters_training_dir"
command+=" -s $reuters_testing_dir"
command+=" -x $dont_follow_roi_exec_dir"
command+=" -r $result_base_dir/raw_data"
command+=" -l"
command+=" -a 6"
command+=" -b 6"
echo "$command" >> $timing_result
echo "$command"
eval $command 2>&1 | tee $tmp_timing_file
extract_timings $tmp_timing_file >> $timing_result
end=`cat $timing_result | wc -l`
start=$((end - 5 + 1))
timing_statistics "Tokenization and TF generation" $start $end >> $timing_result
# End of raw material preparation

# 1. dont_follow_roi
# 1.1 dont_follow_roi.noES
echo 'dont_follow_roi.noES' \
    | tee -a $timing_result >> $performance_result
run_noES "dont_follow_roi" $dont_follow_roi_exec_dir

# 1.2 dont_follow_roi.various_ES_percentages
echo 'dont_follow_roi.various_ES_percentages' \
    | tee -a $timing_result >> $performance_result
run_various_ES_percentages "dont_follow_roi" $dont_follow_roi_exec_dir

# 1.3 dont_follow_roi.crossval.noES
echo 'dont_follow_roi.crossval.noES' \
    | tee -a $timing_result >> $performance_result
run_crossval_noES "dont_follow_roi" $dont_follow_roi_exec_dir

# 1.4 dont_follow_roi.crossval.various_ES_percentages
echo 'dont_follow_roi.crossval.various_ES_percentages' \
    | tee -a $timing_result >> $performance_result
run_crossval_various_ES_percentages "dont_follow_roi" $dont_follow_roi_exec_dir

# 2. follow_roi
# 2.1 follow_roi.noES
echo 'follow_roi.noES' \
    | tee -a $timing_result >> $performance_result
run_noES "follow_roi" $follow_roi_exec_dir

# 2.2 follow_roi.various_ES_percentages
echo 'follow_roi.various_ES_percentages' \
    | tee -a $timing_result >> $performance_result
run_various_ES_percentages "follow_roi" $follow_roi_exec_dir

# 2.3 follow_roi.crossval.noES
echo 'follow_roi.crossval.noES' \
    | tee -a $timing_result >> $performance_result
run_crossval_noES "follow_roi" $follow_roi_exec_dir

# 2.4 follow_roi.crossval.various_ES_percentages
echo 'follow_roi.crossval.various_ES_percentages' \
    | tee -a $timing_result >> $performance_result
run_crossval_various_ES_percentages "follow_roi" $follow_roi_exec_dir

# End of experiment

# Package the experiment result
cd $result_base_dir
echo -e "\n[PACKAGING RESULT]"
echo -n "Tokenization and TF data"
echo -n " (`date`)... "
find raw_data -print0 \
    | cpio -o0 \
    | xz --best > tokenization_and_TF_calculation_result-with_stop_list.cpio.xz

echo -n "dont_follow_roi.crossval.various_ES_percentages"
echo -n " (`date`)... "
find result.dont_follow_roi.crossval.*.ES_percentage.* -print0 \
    | cpio -o0 \
    | xz --best > result.dont_follow_roi.crossval.various_ES_percentages.cpio.xz

echo -n "dont_follow_roi.crossval.noES"
echo -n " (`date`)... "
find result.dont_follow_roi.crossval.*.P.* -print0 \
    | cpio -o0 \
    | xz --best > result.dont_follow_roi.crossval.noES.cpio.xz

echo -n "dont_follow_roi.various_ES_percentages"
echo -n " (`date`)... "
find result.dont_follow_roi.[0-9]*.ES_percentage.* -print0 \
    | cpio -o0 \
    | xz --best > result.dont_follow_roi.various_ES_percentages.cpio.xz

echo -n "dont_follow_roi.noES"
echo -n " (`date`)... "
find result.dont_follow_roi.P.* -print0 \
    | cpio -o0 \
    | xz --best > result.dont_follow_roi.noES.cpio.xz

echo -n "follow_roi.crossval.various_ES_percentages"
echo -n " (`date`)... "
find result.follow_roi.crossval.*.ES_percentage.* -print0 \
    | cpio -o0 \
    | xz --best > result.follow_roi.crossval.various_ES_percentages.cpio.xz

echo -n "follow_roi.crossval.noES"
echo -n " (`date`)... "
find result.follow_roi.crossval.*.P.* -print0 \
    | cpio -o0 \
    | xz --best > result.follow_roi.crossval.noES.cpio.xz

echo -n "follow_roi.various_ES_percentages"
echo -n " (`date`)... "
find result.follow_roi.[0-9]*.ES_percentage.* -print0 \
    | cpio -o0 \
    | xz --best > result.follow_roi.various_ES_percentages.cpio.xz

echo -n "follow_roi.noES"
echo -n " (`date`)... "
find result.follow_roi.P.* -print0 \
    | cpio -o0 \
    | xz --best > result.follow_roi.noES.cpio.xz
# End of packaging
