#!/bin/bash

# Must be changed to reproduce a particular result
rseeds_file=/tmp/experiment_ground/rseeds
# End of parameter that must be change to produce a particular result

# Modifiable without changing reproducibility
rseeds_out_file=/tmp/experiment_ground/rseeds.output
timing_result=/tmp/experiment_ground/timing.csv
performance_result=/tmp/experiment_ground/performance.csv

thread_count=4

rseed_index_file=/tmp/experiment_ground/rseed_index_file.txt
reuters_training_dir=/tmp/Reuters21578-Apte-90Cat/training
reuters_testing_dir=/tmp/Reuters21578-Apte-90Cat/test
dont_follow_roi_exec_dir=/tmp/tc_rocchio/dont_follow_roi
follow_roi_exec_dir=/tmp/tc_rocchio/follow_roi
driver_exec_file=$dont_follow_roi_exec_dir/driver.sh
result_base_dir=/tmp/experiment_ground
tmp_timing_file=$result_base_dir/timing.txt
tmp_performance_file=$result_base_dir/perf.csv
# End of modifiable parameters that do not affect reproducibility

# Changing the following alters how the experiment is conducted
various_ES_percentages='30 40 50 60'
valset_count=2
valset_percentage=30
reported_cats='earn acq money-fx grain crude trade interest ship wheat corn'
# End of parameters that alters how the experiment is conducted

# Setting up default rseeds
if [ ! -f $rseeds_file ]; then
    # for dont_follow_roi.crossval.various_ES_percentages
    for ((i = 1; i <= $valset_count; i++)); do
        for percentages in $various_ES_percentages; do
            echo '-1' >> $rseeds_file # crossval rseed
            echo '-1' >> $rseeds_file # ES rseed
        done
    done
fi
# End of setting up default rseeds

echo 1 > $rseed_index_file
function next_rseed {
    rseed_index=`cat $rseed_index_file`
    if [ $(cat $rseeds_file | wc -l) -lt $rseed_index ]; then
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
    sed -e 's|\(.\+\) \[\([.0-9]\+\)s .\+\([.0-9]\+\)% .\+ of \(.\+\)\]|\1\t\2\t\3\t\4|' $1
}

# $1 dont_follow_roi/follow_roi
# $2 exec_dir
function run_crossval_various_ES_percentages {
    echo "[Running $1.crossval.various_ES_percentages] `date`"
    for percentage in $various_ES_percentages; do
        echo "$1.crossval.ES_percentage.$percentage" \
            | tee -a $timing_result >> $performance_result

        final_result_dir='$result_base_dir'
        final_result_dir+='/result.$1.crossval.$i.ES_percentage.$percentage'

        for ((i=1; i <= $valset_count; i++)); do
            echo "i=$i percentage=$percentage `date`"
            mkdir $result_base_dir/result/
            ln -s ../raw_data/training/ $result_base_dir/result/
            ln -s ../raw_data/testing $result_base_dir/result

            # The following rseeds must be obtained in this order
            cross_rseed=$(next_rseed)
            ES_rseed=$(next_rseed)
            # End of obtaining rseeds

            $driver_exec_file \
                -x $2 \
                -r $result_base_dir/result \
                -t $reuters_training_dir \
                -s $reuters_testing_dir \
                -a 2 \
                -J $thread_count \
                -l \
                -P $percentage \
                -S $ES_rseed \
                -R $cross_rseed \
                -D \
                -V $valset_percentage 2>&1 | tee $tmp_timing_file

            # Extracting timing information
            extract_rseeds $tmp_timing_file >> $rseeds_out_file
            echo ''$driver_exec_file \
                -x $2 \
                -r $result_base_dir/result \
                -t $reuters_training_dir \
                -s $reuters_testing_dir \
                -a 2 \
                -J $thread_count \
                -l \
                -P $percentage \
                -S $ES_rseed \
                -R $cross_rseed \
                -D \
                -V $valset_percentage >> $timing_result
            extract_timings $tmp_timing_file >> $timing_result
            # End of extracting timing information

            rm -r $result_base_dir/result/crossval # due to being reproducible
            mv $result_base_dir/result `eval echo $final_result_dir`
        done

        # Extract performance data
        rm -f $tmp_performance_file

        # Global performance
        echo 'Global Performance' >> $performance_result
        echo -n 'nth cross-validation set' >> $performance_result
        echo -ne '\tMacro Average\tMicro Average' >> $performance_result
        echo -e '\tMicro Average f1\tAverage BEP' >> $performance_result
        echo -e '\tPrecision\tRecall\tPrecision\tRecall' >> $performance_result
        for ((i=1; i <= $valset_count; i++)); do
            tail -n 1 `eval echo $final_result_dir`/perf_measure.txt \
                | sed -e 's%.*%'$i'\t&%' \
                >> $performance_result
        done
        end=`cat $performance_result | wc -l`
        start=$((end - valset_count + 1))
        echo -n '=AVERAGE($F$'$start':$F$'$end')' >> $tmp_performance_file
        echo -e '\t=STDEV($F$'$start':$F:$'$end')' >> $tmp_performance_file
        echo '' >> $performance_result

        # Per category performance
        for cat in $reported_cats; do
            for ((i=1; i <= $valset_count; i++)); do
                echo "Performance on Category $cat" >> $performance_result
                echo -n 'nth cross-validation set' >> $performance_result
                echo -e '\ta\tb\tc\tPrecision\tRecall\tf1\tBEP' \
                    >> $performance_result
                grep -m 1 -- ^$cat \
                    `eval echo $final_result_dir`/perf_measure.txt \
                    | sed -e 's%.*%'$i'\t&%' \
                    >> $performance_result
            done
            end=`cat $performance_result | wc -l`
            start=$((end - valset_count + 1))
            echo -ne '=AVERAGE($F$'$start':$F$'$end')' >> $tmp_performance_file
            echo -e '\t=STDEV($F$'$start':$F:$'$end')' >> $tmp_performance_file
            echo '' >> $performance_result
        done

        # Putting in the statistics
        sed -e 's%.*%\t\t\t\t\t\t\t\t\t\t&%' \
            $tmp_performance_file >> $performance_result
        # End of extracting performance data
    done
}

echo "[Preparing raw material] `date`"
echo 'Tokenization and TF generation' > $timing_result
echo ''$driver_exec_file \
    -t $reuters_training_dir\
    -s $reuters_testing_dir \
    -x $dont_follow_roi_exec_dir \
    -r $result_base_dir/raw_data \
    -l \
    -a 0 \
    -b 1 >> $timing_result
$driver_exec_file \
    -t $reuters_training_dir\
    -s $reuters_testing_dir \
    -x $dont_follow_roi_exec_dir \
    -r $result_base_dir/raw_data \
    -l \
    -a 0 \
    -b 1 2>&1 | tee $tmp_timing_file
extract_timings $tmp_timing_file >> $timing_result
echo ''$driver_exec_file \
    -t $reuters_training_dir\
    -s $reuters_testing_dir \
    -x $dont_follow_roi_exec_dir \
    -r $result_base_dir/raw_data \
    -l \
    -a 6 \
    -b 6 >> $timing_result
$driver_exec_file \
    -t $reuters_training_dir\
    -s $reuters_testing_dir \
    -x $dont_follow_roi_exec_dir \
    -r $result_base_dir/raw_data \
    -l \
    -a 6 \
    -b 6 2>&1 | tee $tmp_timing_file
extract_timings $tmp_timing_file >> $timing_result

echo 'dont_follow_roi.crossval.various_ES_percentages' \
    | tee -a $timing_result > $performance_result
run_crossval_various_ES_percentages dont_follow_roi $dont_follow_roi_exec_dir

exit 0

# Package the experiment result
cd $result_base_dir
find result.dont_follow_roi.crossval.*.ES_percentage.* -print0 \
    | cpio -o0 \
    | xz --best > result.dont_follow_roi.crossval.various_ES_percentages.cpio.xz
# End of packaging
