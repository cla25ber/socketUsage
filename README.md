# socketUsage

This project was made as an assignement for the "Laboratorio II" course of the second year (2022/2023) of the Computer Science faculty of the University of Pisa.
During the course we learned through the C language many useful things, that were shown in the project:
  -System calls: what they are and the interaction the user can have with the operating system; we saw most notably system calls on input and output to interact with files and directories.
  -Processes and threads: their use in concurrent programming, data structures such as semaphores, mutex and condition variables to avoid deadlock and starvation and synchronize them. 
  -Interprocess communication: pipes, sockets, signals and shared memory to make processes communicate.
The final project consisted in an assignement that made use of all these notions.


# requirements

The project's assignement was as follows:
Starting from a directory given as input, the programme visits it and if it finds a file with the ".dat" extension it gets examinated, while if it finds a directory it gets visited recursevely.
When it finds a file with the ".dat" extension, the programme analyzes it and calculates the average and the standard deviation of all the numbers in it.
The output is a table which contains 4 values: n (how many numbers the file has), avg (their average), std (their stadard deviation), and file (the name of the file).
The inputs are the name of the directory in which to start, and a number W that describes how many threads examine and calculates the files; the inputs are passed through command line.
The programme must use 2 processes, a Master process, which is the one that examines the files using W thread workers; and a Collector process, which receives the results from the Master through a socket.
The Master process and its threads work following a producer-consumer paradigm, in fact the Master process must give the files' name through a shared data structure which must be protected with mutual exclusion.
The Collector process takes care of the output.
A Makefile must be made so that by using the "make" command the programme compiles; it must contain these phonies:
  test1: the programme is single-threaded,
  test2: the programme has 5 worker threads,
  test3: the programme has 5 worker threads and valgrind is executed.
It can be assumed that a file's name is not longer than 255 characters.


# file usage

exam.c
  This is the main file, here the Master process creates the Collector process, all the worker threads, sets up the socket connection and sets up the shared data scructure (which will be an unlimited queue).
  The Master process explores the directory given in input and, through an unbounded queue, gives to the worker threads the files to analyze.
  The worker threads then calculate the number of numbers, their average and their standard deviation, which then are formatted in a string that is sent to the Collector process via a socket.
  The Collector process then organizes all the informations in a table and prints it.

unbQueue.h
  This file is the library which contains only the definition of the functions used to manage the synchronized unbounded queue and the definition of the queue's structure.

unbQueue.c
  In this file there is the code for the library's functions.
  The queue operates in a peculiar way, in fact "head" points to a fake node which is, logically, not included in the queue.
  With "logically" I mean that even though by looking at the queue on a low level the node and its information is still present in the memory, by looking at a high level the queue operates without considering it.
  When a "pop" is executed the fake node is eliminated and "freed", while the first real node becomes the fake node, which means that it gets logically eliminated.
  This implementation makes it easier to operate on the queue, in fact logically the queue can be empty, but it will always contain at least one node, the fake one.
  By always having a node present, when we push some data it will always be the "next" of something else, even if the queue was logically empty before.
  This means that when pushing a node to an empty queue, it will be the "next" of the fake node, so we eliminated the possible condition of having a node that is not a "next", and saved up some code.
  It is also safer, because even if the queue is empty the "head" and "tail" pointers will never be NULL.
  This addition is just one more node to the queue, so it has a O(1) cost.

Makefile
  The targets in the makefile operate as follows:
  exam: compiles the exam.c and unbQueue.c files, and creates the executable.
  test1, test2, test3: do respectively the test cases required by the assignement.
  clean: eliminates the executable.

prova1.dat, prova2.dat, provadir
  These are files and directories that are used to test the programme.


# license

This project is licensed under the terms of the MIT License.
License Summary:
The MIT License is an extremely permissive open-source license that allows anyone to use, modify, and distribute the software without significant restrictions. The only requirements are that the copyright and the original license information must be retained in all copies of the software.

For further informations, please refer to the LICENSE file in the repository.


# contacts

e-mail:   cla.bernardoni25@gmail.com
linkedin: www.linkedin.com/in/claudio-bernardoni-b08b66267
