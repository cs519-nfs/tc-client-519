import re
batches = [128, 256, 512, 1024]
batch_to_loc = {
    128: 0,
    256: 1,
    512: 2,
    1024: 3
}
def clean_average(file):
    benchmarks = [{}, {}, {}]
    with open(file) as f:
        f.readline()
        f.readline()
        
        separations = [1, 10, 20, 40]
        type = 0
        batch = 0
        bench_iter = 0
        sep_iter = 0
        bench = 0
        type_iter = 0 

        for s in f:
            if "Average" in s:
                s = s.strip("\n")
                #print(s)
                if bench > 0:
                    print((batches[batch], type, bench_iter, bench, separations[sep_iter]))
                    benchmarks[bench][(type, batches[batch], separations[sep_iter])] = s
                    sep_iter = (sep_iter + 1) % 4
                    batch = (batch + 1) % 4 if sep_iter == 0 else batch
                    type_iter = type_iter + 1 if type_iter+1 < 16 else 0
                    bench_iter = bench_iter + 1 if bench_iter+1 < 32 else 0
                    if type_iter == 0:
                        type = 0 if type == 1 else 1
                else:
                    print((batches[batch], type, bench_iter, bench))
                    benchmarks[bench][(type, batches[batch], separations[sep_iter])] = s
                    batch = (batch + 1) % 4
                    bench_iter = bench_iter + 1 if bench_iter+1 < 8 else 0
                    if batch == 0:
                        type = 0 if type == 1 else 1

                bench = bench + 1 if bench_iter == 0 else bench

    return benchmarks[0], benchmarks[1], benchmarks[2]

#type, batch, sep
def dump_to_file(benchmark, name):
    
    batch_groups = [[],[],[],[]]
    for k in benchmark.keys():
        for k2 in benchmark.keys():
            if k is not k2:
                if k[1] == k2[1] and k[2] == k2[2]:
                    index = batch_to_loc[k[1]]
                    if k[0] == 0:
                        batch_groups[index].append((k, benchmark[k], k2, benchmark[k2]))
                    else:
                        batch_groups[index].append((k2, benchmark[k2], k, benchmark[k]))

                    benchmark.pop(k)
                    benchmark.pop(k2)
    

    for batch, i in zip(batch_groups, range(len(batch_groups))):
        batch_groups[i] = sorted(batch, key=lambda x: x[-2][-1])

    with open(name+".txt", 'w+') as f:
        for batch in batch_groups:
            for entry in batch:
                f.write(str(entry) + "\n")

l = []
b11, b12, b13 = clean_average("benchmark1.txt")
b21, b22, b23 = clean_average("benchmark2.txt")
b31, b32, b33 = clean_average("benchmark3.txt")

l.append(b11)
l.append(b12)
l.append(b13)
l.append(b21)
l.append(b22)
l.append(b23)
l.append(b31)
l.append(b32)
l.append(b33)

for x, i in zip(l, range(len(l))):
    dump_to_file(x, str(i))
