#!/bin/bash -x
echo "Benchmarking"
echo "--------------------"
batches=(128 256 512 1024)
separations=(1 10 20 40)
path="alice.txt"

benchmark () {
	sum=0
	for i in {1..5};
	do
		sudo rm -rf /tcserver/new_test/ && mkdir /tcserver/new_test
		sleep 2
		if [[ $1 -eq 1 ]]
		then
			res=$(sudo ./rw_benchmark_$1 $2 $3)
			echo $res
			sum=$((sum+res))
		else
			res=$(sudo ./rw_benchmark_$1 $2 $3 $4 $path)
			echo $res
			sum=$((sum+res))
		fi
	done
	avg=$((sum/5))
	echo "Average $avg"
	echo "====================="
	return $average
}

#default - 0
#mixed   - 1
for bench in {1..3};
do
	for type in {0..1};
	do
		for batch in ${batches[@]}; 
		do
			if [[ $bench -eq 1 ]]
			then
				echo "Running benchmark $bench in type $type with batch $batch"
				echo "--------------------------------------------------------"
				benchmark $bench $batch $type
			else
				for separation in ${separations[@]};
				do
					echo "Running benchmark $bench in type $type with batch $batch and $separation separations"
					echo "------------------------------------------------------------------------------------"
					benchmark $bench $batch $type $separation
				done
			fi

		done
	done
done
