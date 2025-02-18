[![Build
Status](https://travis-ci.org/sbu-fsl/txn-compound.svg?branch=master)](https://travis-ci.org/sbu-fsl/txn-compound)

About
=====
This "txn-compound" project is short for "transactional compound".  The goal of
this project is to achieve greater performance by leveraging NFSv4's compound
procedures, which is currently under-utilized as we found in our SIGMETRICS'15
paper "Newer Is Sometimes Better: An Evaluation of NFSv4.1". Available at
https://www.fsl.cs.sunysb.edu/docs/nfs4perf/nfs4perf-sigm15.pdf

This project exposes a vectorized file-system API that is discussed in a
[FAST2017 paper][vNFS-talk] entitled ["vNFS: Maximizing NFS Performance with
Compounds and Vectorized I/O"][vNFS-pdf].  In the vNFS paper, the names of the
API functions are in the form of ``vread`` instead of ``tc_readv`` (as in this
repository) to avoid discussion of transaction (which is not part of the
vNFS paper).

In a nutshell, the biggest reason why compound procedures are practically
ineffective is the lower-level nature of POSIX file-system API.  Therefore, in
this project, we will supplement POSIX with higher-level APIs that can take
full advantage of compound procedures.  Changing or adding APIs are always a
scary thing, but having the choice for something different is always better
than "no choice."

The project will be implemented as a user-space file-system library with the
API defined in [tc_client/include/tc_api.h](tc_client/include/tc_api.h).  Right
now, we are implementing two implementations of the API: TC_NFS4 and TC_POSIX.
The TC_NFS4 will implement the API using NFS4's compound procedures whenever
possible, whereas TC_POSIX just translates the higher-level functions to
lower-level POSIX functions.

In theory, transactional compounds can be initiated by applications in storage
client, then be transfered/translated all the way down (through network, OS, and
the deep storage stack) to hardware, such as a Fusion-IO device with internal
transactional support.  Although, the project currently focus on only the
client and API part of transactional compounds, in the future, we would like to
push txn-compound all the way down to the right place, no matter it is the
NFS-Server, the in-kernel file-system, or the storage devices.

Get Started
===========
Note: Currently, the project has only been tested under Linux, or more
specifically, CentOS 7 and Ubuntu 16.

Prerequisite
------------
To compile and run the projects, you need CMake, G++, Jemalloc, Google Test,
Google Mock, ........................... Life will be so much better if we
have a package manager like Maven in the C/C++ world :-(

So, the simplest way is to use the public Docker image built for this project
in [Docker Hub](https://hub.docker.com/r/mingchen/tc-client/)

Alternatively, we can create a CentOS VM, and then execute
[`scripts/install-dependency.sh`](scripts/install-dependency.sh).  A similar
script exists for Ubuntu 16 at
[`scripts/install-dependency-ubuntu16.sh`](scripts/install-dependency-ubuntu16.sh).

Build
-----

        cd tc_client
        mkdir debug
        cd debug
        sudo -E cmake -DCMAKE_BUILD_TYPE=Debug ..
        make

Install
-------
Assuming staying in the debug directory created above:

        sudo make install && sudo make install_manifest.txt

Configure
---------
All configurations are done by editing the config file at
"txn-compound/config/tc.ganesha.conf".

1. start an NFS server (e.g., NFS-Ganesha), and update its IP in the config
   file.

2. configure the NFS server to export a directory called "/vfs0", or update the
   exported directory (default to "/vfs0") accordingly in the config file.

3. create the test file in the exported directory, or update the test file path
   to an existing file in <txn-compound>/tc_client/MainNFSD/tc_test_read.c

        mkdir -p /vfs0/tcdir
        echo "hello txn-compound" > /vfs0/tcdir/abcd

Run
---
Please check "dmesg" and the log file at "/tmp/tc_test_read.log":

        cd  debug/MainNFSD
        sudo ./tc_test_read


Code tree
=========

NFS-Ganesha
-----------
The source code of txn-compound is largely adapted from NFS-Ganesha,
particularly PROXY_FSAL.  So this repository contains many files from
NFS-Ganesha that are not really needed here; they will be gradually cleaned up
in the future.

NFS-Ganesha is an NFSv3,v4,v4.1 fileserver that runs in user mode on most
UNIX/Linux systems.  It also supports the 9p.2000L protocol.

For more information, consult the [project wiki](https://github.com/nfs-ganesha/nfs-ganesha/wiki).

Examples
--------
The examples show usage of TC APIs, and usually perform FS operations with much
less RPC than a standard POSIX NFS client.

- Open, read, and close a file in one RPC:
[tc_client/MainNFSD/tc_test_read.c](tc_client/MainNFSD/tc_test_read.c)

- Create, write, and close a file in one RPC:
[tc_client/MainNFSD/tc_test_write.c](tc_client/MainNFSD/tc_test_write.c)

- Creating a deep directory and all its ancestor directories with only two
RPCs: [tc_client/MainNFSD/tc_test_mkdir.c](tc_client/MainNFSD/tc_test_mkdir.c)

- Creating multiple directories with only one RPC:
[tc_client/MainNFSD/tc_test_mkdirs.c](tc_client/MainNFSD/tc_test_mkdirs.c)

- Listing contents of multiple directories with one RPC:
[tc_client/MainNFSD/tc_test_listdirs.c](tc_client/MainNFSD/tc_test_listdirs.c)

- Removing multiple files with one RPC:
[tc_client/MainNFSD/tc_test_remove.c](tc_client/MainNFSD/tc_test_remove.c)

- Moving multiple files with one RPC:
[tc_client/MainNFSD/tc_test_rename.c](tc_client/MainNFSD/tc_test_rename.c)

LICENSE
=======
Most code in this repo has [LGPL license](tc_client/LICENSE.txt).  However, a
small number utility files has BSD license, for example,
[tc_client/util/slice.h](tc_client/util/slice.h).

Contribution
============
The project is in a very early stage; any help is greatly appreciated.
Looking forward to your git push notification :-)

[vNFS-talk]: https://www.usenix.org/conference/fast17/technical-sessions/presentation/chen
[vNFS-pdf]: http://www.fsl.cs.sunysb.edu/docs/nfs4perf/vnfs-fast17.pdf

Running Test Files
============
After running the server and compiling the test files for the client, copy the following scripts
into the debug/MainNFSD directory:
[tc_client/gather_data.sh]
[tc_client/gdcopy.sh]
[tc_client/benchmark.sh]

Then run for the data for figure 1 a and b:
```
		sudo bash gather_data.sh
```
and for figure 1 c run:
```
		sudo bash gd_copy.sh
```
gather_data.sh runs tc_test_writev multiple times with different parameters which include the number 
of write operations and whether or not to send them as single or multiple vectors.

gdcopy.sh runs tc_test_rw in a similar fashion to gather_data.sh but only takes in the number of 
read-write operations to send as multiple vectors (not singular vectors).

All other figure data is generated by:
```
		sudo bash benchmark.sh
```
Be warned that benchmark.sh can take quite a while to run as it uses very large files and also tests it for the slower
multivector requests. The file alice.txt (tc_client/MainNFSD/alice.txt) will also need to be copied into debug/MainNFSD unless the
path to the file is changed in the benchmarking script. It uses the rw_benchmark_1.c, rw_benchmark_2.c and rw_benchmark_3.c files 
respectively to generate data. The first takes in only the number of vectors, and the whether or not to send it as multiple 
or via our new vector implementation and only uses the data "Hello world". The second does the same except it take in the 
extra paramater for the number of benches as mentioned in the final report. The final takes in a path for a file, which for
benchmarking purposes was alice.txt, and is the slowest to run. 

Bugs
============
As mentioned in the report, there are a few reproducible bugs.
After running the server, and compiling the test files for the client, to reproduce the first bug run:

		sudo ./tc_writev_no_vec 3 1 0 

The first parameter represents the number of requests, here we try 3 and 4 as sometimes the behaviour does not show itself
if the number is too low. The second chooses either compound vector or multiple vectors to send as
requests. The third parameter is the important one as it enables/disables vector transactions. We are disabling it
in this case, which will begin to cause this strange behaviour. The client will occasionally claim that the files
are written (they can also be read from within in the same program so they are clearly there) but when observing
[/tcserver/test] the files are not visible. Using

	ll /tcserver/test

will show that there are definitely files in the directory. There may also be some garbage files in the debug/MainNFSD
directory. The success message is also inconsistent as sometimes it will fail.
Running the same command with the arguments 1 1 0 also works tentatively though some of the code will have to be
uncommented to see this happening.

The second bug is produced through the same command but with the parameters 1 1 1. This enables regular support 
but this crashes the server. Unlike our other benchmark which does not use open and close, this one uses open 
which may in part be the cause.

The write and mkdir issues can be replicated manually.

