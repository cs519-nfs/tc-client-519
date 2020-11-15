echo "Gathering data "
echo "--------------------------"
sleep 2
num_iters=(128 512 256 1024)
# 1 = big vector
# 0 = multiple vectors
for type in {0..1};
do
    for num in ${num_iters[@]};
    do
    echo "Data for: $num files for version: $type"
        for i in {1..5};
        do
            ./tc_test_writev $num $type $i
        done
    done
done
