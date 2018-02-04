#!/bin/bash
failure_count=0;
for ((j=0;j<5000;j++)); do
    temp=$((RANDOM%$1));
    matrix_size=$((temp*8)); # 1st parameter
    iterations=$((RANDOM%$2)); # 2nd parameter
    output1=$(./seq-rb ${matrix_size} ${iterations})
    output2=$(./mt-rb ${matrix_size} ${iterations})
    echo "matrix_size: $matrix_size , iterations: $iterations, serial o/p: $output1, multi o/p: $output2"
    if [ ! "$output1" == "$output2" ]; then
	((failure_count++))
    fi
done
echo "failure count: $failure_count"
