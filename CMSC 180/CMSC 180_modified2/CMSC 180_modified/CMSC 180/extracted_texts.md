--- START OF CMSC 180 Lab01 - 2025-2026-2.pdf ---
CMSC 180
Introduction to Parallel Computing
Second Semester AY 2025-2026
Laboratory Research Problem 01 (LRP01)
Computing the Serial Solution to the Min-Max Transformation of 
Elements in a Column of a Matrix
Introduction
Given a m  n matrix X with m rows and n columns, the Min-Max Transformation (MMT) of X is a 
matrix T such that
Tij = (Xij – min(Xj)) / (max(Xj) – min(Xj)) (Equation 1)
where  min(Xj) and  max(Xj) are, respectively, the minimum and the maximum element of the  jth 
column of X, for all i = 1...m and  j = 1...n.
Research Question 1: What do you think is the complexity of solving the MMT of a n  n square 
matrix X? (hint: CMSC 142)
Research Activity 1 : Write a computer program using the programming language of your choice  
for computing the MMT of an n  n  square matrix X. In other words, transform Equation 1 above  
into a computer program given X.
How to do it? 
1. Write a function mmt() that accepts as parameters the matrix X, and transforms X in place (i.e., 
you are not to create a temporary matrix to hold intermediate computation). For example, in  
pseudocode:
function mmt(X: matrix, m: integer, n: integer): matrix
begin
for i:=1 to n do
for j:=1 to m do
begin
X[i,j]:= {see equation 1 above};
end;
end;
mmt:=X;
end;
2. Write the main program lab01 that includes the following: 
(1) Read n as a user input (maybe from a command line or as a data stream);
(2) Create a non-zero n  n  square matrix X whose elements are assigned with random integers 
(make sure that any integer i  0);
(3) Take note of the system time time_before;
(4) transform X via a call to mmt(X, n, n);
(5) Take note of the system time time_after;
LRP01 CMSC 180: Introduction to Parallel Computing p. 1/2

(6) Obtain the elapsed time time_elapsed:=time_after – time_before; 
(7) output time_elapsed;
For example, for computing the MMT of a 100100 square matrix X:
$ lab01 < 100
$ time elapsed: 10.2345 seconds 
3. Fill in the following table with your time readings:
n
Time Elapsed (seconds) Average
Runtime
(seconds)
Complexity*
Run 1 Run 2 Run 3 Based on
n = 100
Based on 
max(n)
100
200
300
400
500
600
700
800
900
1,000
2,000
4,000
8,000
16,000
20,000
*What does your answer to Research Question 1 say, but converted into a time with respect to n = 100 as well as 
to max(n)?
Research Question 2:  Were you able to run up to n > 20,000? If so, can you make it higher to  
50,000 or even 100,000? If not, why do you think so and what do you need to do to make it so?
4. Using a graphing software (such as LibreOffice Calc), create a line graph of n versus Average 
Runtime obtained from the Table above. On the same graph, plot n versus Complexity as well 
(both based on n = 100 and on max(n) at least up to the n where your program worked).
Research Question 3: Do the lines agree, at least in the form? If not, provide an explanation why  
so?
Research Question 4: Discuss ways on how we can make it better (lower average runtime) without 
using any extra processors or cores (notice that the word “ways” is in plural form).
 CC BY-NC-SA 2026 Jaderick P. Pabico/ICS/UPLB
LRP01 CMSC 180: Introduction to Parallel Computing p. 2/2
--- END OF CMSC 180 Lab01 - 2025-2026-2.pdf ---

--- START OF CMSC 180 Lab02 - 2025-2026-2.pdf ---
CMSC 180
Introduction to Parallel Computing
Second Semester AY 2025-2026
Laboratory Research Problem 02 (LRP02)
Runtime-efficient Threaded Min-Max Transformation of Elements in a Column of a Matrix
Introduction
Given an  m   n matrix  X with  m rows and  n columns, an  m   n matrix  T holds the Min-Max  
Transformation (MMT) of the elements in columns of X as in Equation 1 of the previous LRP01.
Research Question 1: What do you think is the complexity of solving the MMT of the columns in  
an n  n square matrix X when using n concurrent processes? The obvious process assignment is  
one column of X for each process.
Research Question 2: What do you think is the complexity of solving the MMT of the columns in  
an  n   n square matrix  X when using  n/2 concurrent processes (what is the obvious process  
assignment here)? What about with n/4 concurrent processes (i.e., process assignment)? What about 
with  n/8 concurrent processes? What about with  n/m concurrent processes, where  n>>m?   Is the  
process assignment still obvious at n/m concurrent processes?
Laboratory Activity 1: Extend the serially efficient computer program that you wrote in LRP01 to  
use process threads to compute the MMTs of the elements in each of the columns of an n  n square 
matrix X. In other words, transform your efficient serial computer program into a (hopefully more  
efficient and faster) threaded computer program, where a thread is a lightweight process.
How to do it? 
1. Write the main program lab02 that includes the following: 
(1) Read n and t as user inputs (maybe from a command line or as a data stream), where n is the 
size of the square matrix, t is the number of threads to create, and n >> t ;
(2) Create a non-zero n  n  square matrix X whose elements are assigned with random non-
zero integers;
(3) Divide your X into t submatrices of size n  n/t each, which we will respectively call as the  
submatrices x1, x2, …, xt;
(4) Take note of the system time time_before;
(5) Create t threads, where in the ith thread call mmt(xi, n, n/t) for all 1 ≤ i ≤ t;← very important
(6) Recreate the correct T from the output of each threaded mmt();← very important
(7) Take note of the system time time_after;
(8) Obtain the elapsed time time_elapsed:=time_after – time_before; 
(9) Output time_elapsed;
 CC BY-NC-SA 2026 Jaderick P. Pabico/ICS/UPLB
LRP02 CMSC 180: Introduction to Parallel Computing p. 1/3

2. Fill in the following table with your time readings:
n t
Time Elapsed (seconds) Average
Runtime
(seconds)Run 1 Run 2 Run 3
25,000 1*
25,000 2
25,000 4
25,000 8
25,000 16
25,000 32
25,000 64
*This should be closed but a little bit higher to the average that you obtained in LRP01.
Research Question 3: Why do you think that t = 1 will be a little bit higher than the average that  
was obtained in LRP01?
Research Question 4: In step (3) in number 1 above, explain what will happen if we divide X into 
n/t  n instead? How are we going to do it so that the same answer can be arrived at?
Laboratory Activity 2: With lab02, repeat the activities in LRP01 for n = 30,000 and n = 40,000. 
Do you think you can now achieve n = 50,000 and even n = 100,000? Try it to see if you can. If you 
were able to do so, why do you think you can now do it? If not yet, why do you think you still can  
not?
3. Using a graphing software for each n, graph t versus Average obtained from the Table above.  
Describe in detail what you have observed. Do you think you can go as far as t = n? If not, what 
about t = n/2? Or, t = n/4? Or, t = n/8?
Laboratory Activity 3 : Repeat laboratory activity 1 but for the division of  X as described in  
Research  Question  4.  What  sort  of  thing  do  you  need  to  do  so  that  this  division  can  be  
implemented? Graph the average as in number 3 above, if obtained.
Solutions to Possible Problems that may Come up
Problem 1 : I do not know how to write threaded programs.
Answer 1 : Is that really a problem?
Problem 2 : The programming language I used for LRP01 does not have a robust library for  
writing threaded codes.
Answer 2 : See Answer 1.
Problem 3 : My [boy|girl]friend broke up with me today and I can not really concentrate on this  
exercise.
Answer 3 : See Answer 1.
Problem 4 : I have not eaten any meal since last meeting.
Answer 4 : Pretend you are a freshman and attend any org's orientation.
LRP02 CMSC 180: Introduction to Parallel Computing p. 2/3
Some Helpful Online Resources (also listed in Google Classroom)
1. For Java: https://beginnersbook.com/2013/03/multithreading-in-java
2. For C: https://www.geeksforgeeks.org/multithreading-c-2
3. For Python: https://realpython.com/intro-to-python-threading
4. For Perl: https://metacpan.org/pod/distribution/perl/pod/perlthrtut.pod
If our favorite PL is not listed here, we must consider adding another PL as our favorite.
 CC BY-NC-SA 2026 Jaderick P. Pabico/ICS/UPLB
LRP02 CMSC 180: Introduction to Parallel Computing p. 3/3
--- END OF CMSC 180 Lab02 - 2025-2026-2.pdf ---

--- START OF CMSC 180 Lab03 - 2025-2026-2.pdf ---
CMSC 180
Introduction to Parallel Computing
Second Semester AY 2025-2026
Laboratory Research Problem 03 (LRP03)
Core-affine Threaded Min-Max Transformation of Elements in a Column of a Matrix
Introduction
Rewrite the program that you wrote in Laboratory Research Problem 02 (LRP02) so that the threads 
that you created each must be ran specifically unto one specific core only. If your computing  
machine has four or eight threads, assign your threads to three or seven cores, respectively.
Research Activity 1: How to do it? 
1. Write the main program lab03 (just rewrite lab02) that includes the following: 
(1) Read n and t as user inputs (maybe from a command line or as a data stream), where n is the 
size of the square matrix, t is the number of threads to create, and n >> t ;
(2) Create a non-zero n  n  square matrix X whose elements are assigned with random integer;
(3) Divide your X into t submatrices of size n  n/t each, x1, x2, …, xt;
(4) Take note of the system time time_before;
(5) Create t threads, where for the ith thread call mmt(xi, n, n/t);← very important
(6) Assign the thread to a core;← very important, this is the additional step in this exercise
(7) Recreate the correct T from the output of each threaded mmt();← very important
(8) Take note of the system time time_after;
(9) Obtain the elapsed time time_elapsed:=time_after – time_before; 
(10) Output time_elapsed;
2. Fill in the following table:
n t
Time Elapsed (seconds) Average
Runtime
(seconds)Run 1 Run 2 Run 3
25,000 1
25,000 2
25,000 4
25,000 8
25,000 16
25,000 32
25,000 64
Research Question 1:  What is the difference of the average time that you obtained for  t = 1  
compared to the average that was obtained in LRP01 and LRP02?
LRP03 CMSC 180: Introduction to Parallel Computing p. 1/2

Research Activity 2: Repeat research activity 1 for n = 30,000 and n = 40,000. Do you think you  
can now achieve n = 50,000 and even n = 100,000? Try it to see if you can. If you were able to do  
so, why do you think you can now do it? If not yet, why do you think you still can not?
3. Using a graphing software for each n, graph t versus Average obtained from the Table above.  
Label the line as “ With core affinity.” Then superimpose the graph that you obtained in LRP02  
and label that line as “ Without core affinity.” Superimpose also the graph that you obtained in  
LRP01 and label it as “ Serial.” Describe in detail what you have observed. Do you think you  
can go as far as t = n? If not, what about t = n/2? Or, t = n/4? Or, t = n/8?
Research Question 2: Did your average time improve compared to LRP01 and LRP02 for every t? 
Which t is the average time better? Statistically the same? Slower? Why?
Research Question 3: Discuss in your own words what will happen if instead you divide X as in  
step (3) above, still into t submatrices, but in a manner where each submatrix is of size n/t  n?
Solutions to Possible Problems that may Come up
Problem 1 : I do not know how to write core-affine threaded programs.
Answer 1 : Is that really a problem?
Problem 2 : The programming language I used for LRP01 and LRP02 does not have a robust  
library for writing core-affine threaded codes.
Answer 2 : See Answer 1.
Some Helpful Online Resources (also listed in Google Classroom)
1. C/C++ Example from bytefreaks.net
2. C++ Threads, Affinity, and Hyperthreading from Dzone
3. Python setaffinity() method from geeksforgeeks.org
4. Perl’s CpuAffinity() from metacpan.org
LRP03 CMSC 180: Introduction to Parallel Computing p. 2/2
--- END OF CMSC 180 Lab03 - 2025-2026-2.pdf ---

--- START OF CMSC 180 Lab04 - 2025-2026-2.pdf ---
CMSC 180
Introduction to Parallel Computing
Second Semester AY 2025-2026
Laboratory Research Problem 04 (LRP04)
Distributing Parts of a Matrix over Sockets
Introduction
Write a program that will create an open socket for sending and receiving data. Allow the program  
to read from the command line the port number to open and listen to. Also, let the program read  
from a configuration file a list of IP addresses and corresponding ports to communicate to. Run  
several instances of the program over several terminals or PCs. If the instances of your program are  
ran on the same PC, then that PC's IP address must be in the configuration file, and each instance  
must open and listen to different ports. If the instances of your program are ran on different PCs,  
then the IP addresses of these PCs must be in the configuration file together with a specific port.  
You define your own port. From among the running instances elect one instance as the master while 
the rest as the slave. The master will create a matrix A and distribute the corresponding submatrices  
to the slaves.
Research Activity 1: How to do it? 
1. Write the main program lab04 that includes the following: 
(1) Read n, p and s as user inputs (maybe from a command line or as a data stream), where n is 
the size of the square matrix, p is the port number, and s is the status of the instance (0 for  
master and 1 for slave);
(2) If s = 0, then
a. Create a non-zero  n   n  square matrix  M whose elements are assigned with random  
non-zero positive integers;
b. Read the configuration file to determine the IP addresses and ports of the slaves and the  
number of slaves t;
c. Divide your M into t submatrices of size n/t  n each. Call the submatrices m1, m2, …, 
and mt, respectively.
d. Take note of the system time time_before;
e. Distribute the  t submatrices to the corresponding  t slaves by opening the port  p and  
initiating communication with the IP and port of each slave;
f. Receive the acknowledgment “ack” from each slave, for all slaves t;
g. Wait when all t slaves have sent their respective acknowledgments;
f. Take note of the system time time_after;
(3) Else if s = 1,
a. Read from the configuration file what is the IP address of the master;
b. Wait for the master to initiate an open port communication with it by listening to the  
port assigned by the configuration file;
c. When the master has initiated, take note of time_before;
d. Receive from the master the submatrix mi assigned to it;
LRP04 CMSC 180: Introduction to Parallel Computing p. 1/2

e. Send an acknowledgment “ack” to the master once the submatrix have been received  
fully;
f. Take note of time_after;
(4) Obtain the elapsed time time_elapsed:=time_after – time_before; 
(5) Output time_elapsed at each instance’s terminal;
(6) Verify that each of the t slaves received the correct submatrix.
(7) Run all instances within one PC only but on different terminals.
2. Fill in the following table with your time readings (only from the master):
n t
Time Elapsed (seconds) Average
Runtime
(seconds)Run 1 Run 2 Run 3
4,000 2
4,000 4
4,000 8
4,000 16
8,000 2
8,000 4
8,000 8
8,000 16
16,000 2
16,000 4
16,000 8
16,000 16
3. Repeat 1 and 2, but run all slave instances in a core-affine way. What happened?
4. Repeat 1 and 2, but run all slave instances on different PCs. What happened?
5. Is your implementation efficient? Did you use any of the communication techniques discussed  
in the lecture? If so, what is it (one-to-many broadcast, many-to-many broadcast, one-to-many  
personalized broadcast, many-to-many personalized broadcast)? If not, why not?
Note that because of 3 and 4, your report must include three tables.
Quiet reactions to possible complaints that may come up, and all other complaints will be ignored.
Complaint 1: I do not know how to write socket programs.
Reaction 1 (to self with sarcasm): I am sure you learned to read the alphabet in the first grade by  
complaining.
Complaint 2: The programming language I used for LRP01 through LRP03 does not have a robust  
library for writing socket codes.
Reaction 2 (annoyed talking to self like an schizophrenic): Yeah, right. Why is your nose getting  
longer, Pinocchio?
 CC BY-NC-SA 2026 Jaderick P. Pabico/ICS/UPLB
LRP04 CMSC 180: Introduction to Parallel Computing p. 2/2
--- END OF CMSC 180 Lab04 - 2025-2026-2.pdf ---

--- START OF CMSC180_Lab03_Report.pdf ---
 
n  t  
Time  Elapsed  (seconds) Average  Runtime  (seconds)  Run  1 Run  2 Run  3 25,000 1 16.882696  16.646522  16.418576  16.649265  25,000 2 14.613822  14.534789  14.820335  14.656315  25,000 4 13.796131  13.219227  13.303665  13.439674  25,000 8 10.622112  11.174998  10.216272  10.671127  25,000 16 10.129116  10.129116  11.816843  10.691692  25,000 32 11.072569  10.583262  10.456921  10.704251  25,000  64  10.381563  10.076244  10.074662  10.177490    Research  Question  1:  What  is  the  difference  of  the  average  time  that  you  obtained  for  t  =  1  
compared
 
to
 
the
 
average
 
that
 
was
 
obtained
 
in
 
LRP01
 
and
 
LRP02?
 
 The  average  time  that  I  obtained  for  t  =  1  in  LRP03  is  slightly  slower  than  LRP01  and  LRP02.  
This
 
happens
 
because
 
LRP01
 
is
 
a
 
serial
 
program
 
with
 
zero
 
overhead.
 
LRP02
 
introduces
 
thread
 
creation
 
overhead,
 
which
 
is
 
why
 
it
 
is
 
slightly
 
slower
 
than
 
LRP01.
 
LRP03
 
is
 
even
 
slightly
 
slower
 
than
 
LRP02
 
because,
 
on
 
top
 
of
 
the
 
thread
 
creation
 
overhead,
 
it
 
also
 
has
 
system
 
call
 
overhead.
 
The
 
program
 
must
 
execute
 
the
 
SetThreadAffinityMask()
 
function
 
to
 
force
 
the
 
thread
 
to
 
run
 
on
 
a
 
specific
 
logical
 
processor.
 
By
 
restricting
 
the
 
thread
 
to
 
a
 
single
 
core,
 
I
 
disable
 
the
 
OS
 
ability
 
to
 
migrate
 
the
 
thread
 
to
 
an
 
idle
 
core
 
if
 
background
 
applications
 
spike
 
CPU
 
usage
 
on
 
that
 
specific
 
core.
               
n  t  
Time  Elapsed  (seconds) Average  Runtime  (seconds)  Run  1 Run  2 Run  3 40,000  1 100.762865  80.962808  80.177616  87.301096  40,000  2 56.645065  57.622343  55.039595  56.435668  
n  t  
Time  Elapsed  (seconds) Average  Runtime  (seconds)  Run  1 Run  2 Run  3 30,000 1 26.719472  30.264519  28.771548  28.58518  30,000 2 24.66894  28.975546  26.061706  26.568731  30,000 4 25.822573  26.42276  22.863536  25.03629  30,000 8 20.067161  20.716201  20.974003  20.585788  30,000 16 19.059442  17.447203  18.939931  18.482192  30,000 32 18.556236  18.984613  19.051165  18.864005  30,000 64  17.291712  16.436813  17.182956  16.970494  
40,000  4 50.919644  52.22604  51.736744  51.627476  40,000  8 43.891709  41.273156  43.526415  42.897093  40,000  16 39.686137  39.335844  38.935961  39.319314  40,000  32 39.011227  37.879344  40.411656  39.100742  40,000  64  38.02279  36.90786  37.345439  37.425363    Do  you  think  you  can  now  achieve  n  =  50,000  and  even  n  =  100,000?  Try  it  to  see  if  you  can.  If  
you
 
were
 
able
 
to
 
do
 
so,
 
why
 
do
 
you
 
think
 
you
 
can
 
now
 
do
 
it?
 
If
 
not
 
yet,
 
why
 
do
 
you
 
think
 
you
 
still
 
can
 
not?
 
 Even  though  I  drastically  reduced  the  computation  time  by  distributing  the  workload,  I  still  cannot  
achieve
 
n
 
=
 
50,000
 
and
 
n
 
=
 
100,000.
 
Similarly
 
to
 
the
 
last
 
exercise,
 
I
 
only
 
was
 
able
 
to
 
run
 
until
 
n
 
=
 
46,000.
 
The
 
problem
 
is
 
not
 
a
 
computational
 
speed
 
but
 
rather
 
strict
 
hardware
 
memory
 
constraints(RAM
 
restrictions).
 
For
 
context,
 
a
 
100,
 
000
 
x
 
100,000
 
single-precision
 
float
 
matrix
 
requires
 
exactly
 
40
 
Gigabytes
 
of
 
contiguous
 
RAM
 
to
 
initialize.
 
Because
 
this
 
massive
 
memory
 
request
 
vastly
 
exceeds
 
my
 
available
 
physical
 
hardware,
 
the
 
Windows
 
OS
 
intervenes
 
and
 
terminates
 
the
 
.exe
 
process
 
to
 
protect
 
the
 
computer
 
from
 
a
 
system
 
crash.
  
  

  
  
Describe
 
in
 
detail
 
what
 
you
 
have
 
observed.
 Observation:  Based  on  the  superimposed  graph,  the  Serial(LRP01)  looks  like  a  high,  flat  
baseline.
 
The
 
threaded
 
implementation
 
(LRP02
 
and
 
LRP03)
 
drop
 
sharply
 
downward
 
initially,
 

proving  a  massive  parallel  speedup.  However,  with  core  affinity(LRP03),   its  line  consistently  
stays
 
slightly
 
above
 
the
 
line
 
of
 
“without
 
core
 
affinity”
 
for
 
all
 
t
 
>
 
1.
 
This
 
proves
 
that
 
manually
 
assigning
 
threads
 
to
 
specific
 
cores
 
incurs
 
a
 
penalty
 
from
 
restricting
 
the
 
OS
 
scheduler
 
from
 
dynamically
 
allocating
 
workloads
 
to
 
idle
 
cores.
 
Furthermore,
 
because
 
the
 
MMT
 
algorithm
 
is
 
a
 
one-pass
 
algorithm(each
 
column
 
is
 
read
 
only
 
once),
 
the
 
rigid
 
core
 
pinning
 
provides
 
zero
 
CPU
 
cache
 
locality
 
benefits
 
to
 
offset
 
the
 
scheduling
 
penalty.
 
  Can  we  go  as  far  as  t  =  n?  What  about  t  =  n/2?  Or,  t  =  n/4?  Or,  t  =  n/8?  
 NO,  we  cannot  go  as  far  as  t  =  n.  Attempting  to  create  25,000  individual  threads,  for  example,  
would
 
crash
 
the
 
program
 
due
 
to
 
OS
 
resource
 
exhaustion
 
and
 
massive
 
thread
 
creation
 
overhead.
 
Even
 
at
 
a
 
significantly
 
smaller
 
fraction
 
like
 
t
 
=
 
n
 
/
 
8,
 
forcing
 
over
 
3,000
 
threads
 
to
 
combat
 
for
 
execution
 
time
 
causes
 
extreme
 
thread
 
contention.
 
This
 
leads
 
to
 
massive
 
context
 
switching
 
overhead.
 
The
 
CPU
 
starts
 
spending
 
significantly
 
more
 
computation
 
time
 
rapidly
 
pausing
 
threads,
 
swapping
 
memory
 
registers,
 
and
 
managing
 
3,000
 
queues
 
than
 
it
 
spends
 
actually
 
doing
 
the
 
MMT
 
math.
 
This
 
is
 
why
 
the
 
graph
 
proves
 
that
 
performance
 
bottoms
 
out
 
optimally
 
near
 
the
 
physical
 
core
 
count,
 
and
 
worsens
 
if
 
too
 
many
 
threads
 
are
 
made.
   Research  Question  2:  Did  your  average  time  improve  compared  to  LRP01  and  LRP02  for  every  
t?
 
Which
 
t
 
is
 
the
 
average
 
time
 
better?
 
Statistically
 
the
 
same?
 
Slower?
 
Why?
   Compared  to  the  LRP01,  the  average  time  improved  massively  for  all  t  >  1  because  the  workload  
was
 
divided
 
across
 
multiple
 
cores.
 
However,
 
compared
 
to
 
LRP02,
 
the
 
average
 
time
 
in
 
LRP03
 
was
 
statistically
 
the
 
same
 
or
 
slower
 
for
 
every
 
t.
 
This
 
happens
 
because
 
of
 
several
 
reasons.
 
One
 
of
 
which
 
is
 
because
 
of
 
the
 
reduced
 
available
 
hardware.
 
In
 
LRP03,
 
the
 
instructions
 
says
 
to
 
intentionally
 
reserve
 
1
 
core.
 
By
 
restricting
 
our
 
threads
 
to
 
only
 
the
 
remaining
 
cores(in
 
my
 
case
 
7
 
because
 
I
 
have
 
8
 
cores),
 
I
 
mathematically
 
gave
 
the
 
program
 
less
 
processing
 
power
 
than
 
LRP02,
 
which
 
utilized
 
100%
 
of
 
the
 
CPU.
 
Connected
 
to
 
that
 
is
 
the
 
second
 
reason
 
why
 
it
 
did
 
not
 
improve,
 
OS
 
scheduler
 
restriction(load
 
balancing).
 
Modern
 
OS
 
are
 
highly
 
optimized
 
to
 
dynamically
 
shift
 
threads
 
to
 
idle
 
cores.
 
By
 
locking
 
threads
 
to
 
specific
 
cores
 
using
 
affinity,
 
we
 
forced
 
them
 
to
 
wait
 
if
 
their
 
assigned
 
core
 
became
 
busy,
 
entirely
 
removing
 
the
 
OS
 
dynamic
 
load
 
balancer.
 
  Research  Question  3:  Discuss  in  your  own  words  what  will  happen  if  instead  you  divide  X  as  in  
step
 
(3)
 
above,
 
still
 
into
 
t
 
submatrices,
 
but
 
in
 
a
 
manner
 
where
 
each
 
submatrix
 
is
 
of
 
size
 
n/t
 
x
 
n?
 
 
The
 
Min-Max
 
Transformation
 
(MMT)
 
formula
 
requires
 
the
 
global
 
minimum
 
and
 
maximum
 
of
 
the
 
entire
 
column
 
to
 
normalize
 
the
 
elements.
 
Dividing
 
the
 
matrix
 
into
 
dimensions
 
of
 
n/t
 
x
 
n
 
means
 
that
 
we
 
are
 
partitioning
 
the
 
matrix
 
horizontally
 
by
 
chunks
 
of
 
rows.
 
If
 
a
 
thread
 
is
 
assigned
 
a
 
chunk
 
of
 
rows,
 
it
 
only
 
possesses
 
an
 
incomplete
 
segment
 
of
 
each
 
column.
 
With
 
this,
 
the
 
thread
 
cannot
 
determine
 
the
 
minimum
 
and
 
maximum
 
of
 
that
 
column
 
to
 
perform
 
the
 
normalization.
 
To
 
solve
 
this
 
problem,
 
exactly
 
like
 
what
 
I
 
have
 
done
 
in
 
the
 
LRP02
 
Row
 
Wise
 
code,
 
I
 
implemented
 
a
 
two-phase
 
approach.
 
The
 
phase
 
1
 
finds
 
the
 
local
 
minimum
 
and
 
maximum
 
for
 
its
 
segment
 
of
 
every
 
column.
 
Then,
 
synchronization
 
happens
 
to
 
gather
 
all
 
the
 
local
 
extremes
 
to
 
determine
 
the
 
true
 
global
 
extremes  for  the  entire  column.  Then,  Phase  2  happens  where  the  MMT  formula  is  applied  to  
their
 
assigned
 
rows
 
using
 
newly
 
synchronized
 
minimum
 
and
 
maximum.
 
Even
 
though
 
this
 
row
 
partitioning
 
requires
 
a
 
complex
 
synchronization
 
phase,
 
it
 
is
 
actually
 
faster
 
than
 
the
 
column-partitioning
 
approach.
 
Because
 
C
 
stores
 
matrices
 
in
 
Row-Major
 
order,
 
assigning
 
threads
 
by
 
rows
 
ensures
 
Spatial
 
Cache
 
Locality.
 
The
 
CPU
 
prefetcher
 
eliminates
 
memory
 
stall
 
cache
 
misses,
 
making
 
the
 
row-wise
 
approach
 
faster
 
despite
 
the
 
synchronization
 
overhead.
 
 
--- END OF CMSC180_Lab03_Report.pdf ---

--- START OF CMSC_180_Lab02_Reyes.pdf ---
Laboratory Research Problem 02 (LRP02)
CMSC 180: Introduction to Parallel Computing
Charles Andrei P. De los Reyes
CD-3L
February 2026
Laboratory Activity 1
n t Run 1 Run 2 Run 3 Average Runtime (seconds)
25,000 1 3.022526 3.062663 2.981693 3.022294
25,000 2 1.626218 1.64152 1.664496 1.644078
25,000 4 1.057191 1.059241 1.090782 1.069071
25,000 8 0.926981 0.935796 0.914016 0.925598
25,000 16 0.9396 0.953335 0.927611 0.940182
25,000 32 0.937773 1.038196 1.14118 1.03905
25,000 64 0.92787 1.054899 1.055495 1.012755
Table 1: Runtime Results forn= 25,000
Research Question 1
What do you think is the complexity of solving the MMT of the columns in an n n square
matrix X when using n concurrent processes? The obvious process assignment is one column
of X for each process.
The complexity of solving MMT of the columns in an n x n square matrix X using n
concurrent processes is O(n). Since each of the n processes is assigned exactly one column,
the computations occur at the same time across all columns. Finding the minimum, finding
the maximum, and transforming the elements in a single column requires iterating through
n rows. The parallel time complexity is reduced from O(n 2) to O(n).
1
Research Question 2
What do you think is the complexity of solving the MMT of the columns in an n x n square
matrix X when using n/2 concurrent processes (what is the obvious process assignment
here)? What about with n/4 concurrent processes (i.e., process assignment)? What about
with n/8 concurrent processes? What about with n/m concurrent processes, where n¿¿m?
Is the process assignment still obvious at n/m concurrent processes?
The asymptotic complexity remains O(n) for constant divisors, but the execution time
scales based on the chunk size assigned to each process.
With n/2 processes, each process handles 2 columns. Time per process is O(2n), which
simplifies to O(n). The obvious assignment is giving each process 2 contiguous columns.
With n/4 processes, each handles 4 columns. Time is O(4n) -¿ O(n).
With n/8 processes, each handles 8 columns. Time is O(8n) -¿ O(n).
With n/m processes (where n≫m), each process handles m columns. The time com-
plexity per process becomes O(m·n).
Research Question 3
Why do you think that t = 1 will be a little bit higher than the average that was obtained
in LRP01?
The average runtime for t=1 will be slightly higher than the serial LRP01 baseline due to
multithreading overhead. Although a single thread performs the exact same O(n2) operations
as the serial program, the threaded version incurs additional operating system costs. These
costs include allocating thread data structures, invoking pthread create to spawn the thread,
performing context switches, and using pthreadjoin to wait for completion. These operations
consume small amounts of CPU time, adding a slight delay not present in a purely serial
execution.
Research Question 4
In step (3) in number 1 above, explain what will happen if we divide X into n/t x n instead?
How are we going to do it so that the same answer can be arrived at?
Dividing the matrix horizontally into rows means each of our threads only sees a small
piece of every column. Because the Min-Max Transformation needs the highest and lowest
values of an entire column to work correctly, a thread cannot get the right answer using
only its assigned row slice. To fix this, we split our program’s logic into two separate stages.
2
In the first stage, each thread scans its assigned rows to find its own ”local” minimum and
maximum for each column. Our program then pauses all threads so the main process can
collect and compare these results to find the true, ”global” minimum and maximum for the
entire matrix. Finally, we send the threads back out to finish the math using those correct
global values, ensuring the final output is identical to our original version.
Laboratory Activity 2
With lab02, repeat the activities in LRP01 for n = 30,000 and n = 40,000.
n = 30,000
n t Run 1 Run 2 Run 3 Average Runtime (seconds)
30,000 1 4.29694 4.544682 4.397183 4.412935
30,000 2 2.449561 2.288231 2.266153 2.334648
30,000 4 1.561157 1.511252 1.522325 1.531578
30,000 8 1.458435 1.421126 1.307328 1.39563
30,000 16 1.328941 1.354156 1.714713 1.465937
30,000 32 1.366818 1.335692 1.412864 1.371791
30,000 64 1.322652 1.321125 1.371993 1.33859
Table 2: Runtime Results forn= 30,000
n = 40,000
n t Run 1 Run 2 Run 3 Average Runtime (seconds)
40,000 1 7.638329 7.762806 7.965945 7.789027
40,000 2 5.912063 4.87867 4.844395 5.211709
40,000 4 3.509715 4.352533 3.524858 3.795702
40,000 8 3.46809 3.174579 3.129556 3.257408
40,000 16 3.477013 2.590637 2.602847 2.890166
40,000 32 2.497827 3.275687 2.821965 2.86516
40,000 64 3.171257 3.185955 2.405998 2.92107
Table 3: Runtime Results forn= 40,000
The threaded program successfully executed for matrix sizes of n = 30,000 and n = 40,000,
continuing to show performance improvements with multiple threads. However, I still cannot
3
achieve n = 50,000 or n = 100,000, as my computer reached its absolute maximum limit at
exactly n = 46,000. This failure is caused by a strict hardware limitation regarding Random
Access Memory (RAM). In C, every number in the matrix requires 4 bytes of memory.
At n = 46,000, the matrix requires about 8.46 GB of continuous RAM, which completely
exhausts my system’s available memory. Attempting n = 50,000 would require 10 GB, and
n = 100,000 would demand 40 GB. Because the operating system does not have enough
free physical memory to handle these massive requests, the memory allocation fails and the
program cannot proceed.
Graph Analysis
Figure 1: t versus Average Runtime
3. Using a graphing software for each n, graph t versus Average obtained from the Table
above. Describe in detail what you have observed. Do you think you can go as far as t = n?
If not, what about t = n/2? Or, t = n/4? Or, t = n/8?
The graph has three main parts. First, the program gets much faster as you go from 1 to
8 threads, which shows the computer is doing a great job splitting up the work. Second, the
speed stays about the same between 8 and 16 threads because the computer has maxed out
its physical limits. Finally, the program actually gets a little slower at 32 and 64 threads.
It is impossible to scale the number of threads as high as t = n, t = n/2, t = n/4, or even
t = n/8. For example, with a matrix size of n = 25,000, just reaching t = n/8 would require
creating 3,125 concurrent threads. Attempting this fails for two major reasons: thread
thrashing and memory exhaustion. The CPU would spend almost all of its processing power
4
managing the massive traffic jam of threads rather than actually solving the matrix math.
Additionally, since the operating system allocates about 8 MB of RAM per thread, trying
to run 25,000 threads (t = n) would demand roughly 200 GB of memory.
Laboratory Activity 3
Repeat laboratory activity 1 but for the division of X as described in Research Question 4.
To implement this row-wise division, we have to change the program’s logic to separate
the scanning step from the math step. We do this by creating two different thread functions:
one to find the local high and low points, and another to actually apply the formula. We
also have to use a synchronization barrier with pthread join between these two stages. This
pause gives the main thread enough time to calculate the true global extremes for the entire
matrix before the final math begins.
n t Run 1 Run 2 Run 3 Average Runtime (seconds)
25,000 1 5.262339 5.375589 7.462539 6.033489
25,000 2 3.534186 3.437617 3.367118 3.446307
25,000 4 2.3577 2.307638 2.41343 2.359589
25,000 8 2.146627 2.147464 2.130004 2.141365
25,000 16 2.174718 2.208385 2.158602 2.180568
25,000 32 2.165366 2.401883 2.943455 2.503568
25,000 64 2.235485 2.219784 2.181626 2.212298
Table 4: Runtime Results Using Row-wise Division
Figure 2: t versus Row-Wise Average Runtime
5
--- END OF CMSC_180_Lab02_Reyes.pdf ---

--- START OF CMSC_180_Lab03_Final.pdf ---
Laboratory Research Problem 03 (LRP03)
CMSC 180: Introduction to Parallel Computing
Charles Andrei P. De los Reyes
CD-3L
March 26, 2026
Table 1: Runtime Results forn= 25,000
n tRun 1 Run 2 Run 3 Average Runtime (seconds)
25,000 1 16.882696 16.646522 16.418576 16.649265
25,000 2 14.613822 14.534789 14.820335 14.656315
25,000 4 13.796131 13.219227 13.303665 13.439674
25,000 8 10.622112 11.174998 10.216272 10.671127
25,000 16 10.129116 11.816843 10.129116 10.691692
25,000 32 11.072569 10.583262 10.456921 10.704251
25,000 64 10.381563 10.076244 10.074662 10.177490
Research Question 1
What is the difference of the average time that you obtained fort= 1com-
pared to the average that was obtained in LRP01 and LRP02?
The average time that I obtained fort= 1 in LRP03 is slightly slower than LRP01 and
LRP02. This happens because LRP01 is a serial program with zero overhead. LRP02 in-
troduces thread creation overhead, which is why it is slightly slower than LRP01. LRP03
is even slightly slower than LRP02 because, on top of the thread creation overhead, it also
has system call overhead. The program must execute theSetThreadAffinityMask()
function to force the thread to run on a specific logical processor. By restricting the
thread to a single core, I disable the OS ability to migrate the thread to an idle core if
background applications spike CPU usage on that specific core.
1
Table 2: Runtime Results forn= 30,000
n tRun 1 Run 2 Run 3 Average Runtime (seconds)
30,000 1 26.719472 30.264519 28.771548 28.585180
30,000 2 24.668940 28.975546 26.061706 26.568731
30,000 4 25.822573 22.863536 26.422760 25.036290
30,000 8 20.067161 20.716201 20.974003 20.585788
30,000 16 19.059442 17.447203 18.939931 18.482192
30,000 32 18.556236 18.984613 19.051165 18.864005
30,000 64 17.291712 16.436813 17.182956 16.970494
Table 3: Runtime Results forn= 40,000
n tRun 1 Run 2 Run 3 Average Runtime (seconds)
40,000 1 100.762865 80.177616 80.962808 87.301096
40,000 2 56.645065 57.622343 55.039595 56.435668
40,000 4 50.919644 51.736744 52.226040 51.627476
40,000 8 43.891709 43.526415 41.273156 42.897093
40,000 16 39.686137 38.935961 39.335844 39.319314
40,000 32 39.011227 40.411656 37.879344 39.100742
40,000 64 38.022790 37.345439 36.907860 37.425363
Do you think you can now achieven= 50,000and evenn= 100,000? Try it to
see if you can. If you were able to do so, why do you think you can now do
it? If not yet, why do you think you still can not?
Even though I drastically reduced the computation time by distributing the workload,
I still cannot achieven= 50,000 andn= 100,000. Similarly to the last exercise, I only
was able to run untiln= 46,000. The problem is not a computational speed but rather
strict hardware memory constraints (RAM restrictions). For context, a 100,000×100,000
single-precision float matrix requires exactly 40 Gigabytes of contiguous RAM to initialize.
Because this massive memory request vastly exceeds my available physical hardware, the
Windows OS intervenes and terminates the.exeprocess to protect the computer from
a system crash.
2
Graph Analysis & Observation
Figure 1: Average Runtime vs Threads (n= 25,000)
Figure 2: Average Runtime vs Threads (n= 30,000)
3
Figure 3: Average Runtime vs Threads (n= 40,000)
Describe in detail what you have observed.
Based on the superimposed graph, the Serial (LRP01) looks like a high, flat baseline.
The threaded implementation (LRP02 and LRP03) drop sharply downward initially, prov-
ing a massive parallel speedup. However, with core affinity(LRP03), its line consistently
stays slightly above the line of ”without core affinity” for allt >1. This proves that
manually assigning threads to specific cores incurs a penalty from restricting the OS
scheduler from dynamically allocating workloads to idle cores. Furthermore, because the
MMT algorithm is a one-pass algorithm (each column is read only once), the rigid core
pinning provides zero CPU cache locality benefits to offset the scheduling penalty.
Can we go as far ast=n? What aboutt=n/2? Or,t=n/4? Or,t=n/8?
NO, we cannot go as far ast=n. Attempting to create 25,000 individual threads,
for example, would crash the program due to OS resource exhaustion and massive thread
creation overhead. Even at a significantly smaller fraction liket=n/8, forcing over
3,000 threads to combat for execution time causes extreme thread contention. This leads
to massive context switching overhead. The CPU starts spending significantly more
computation time rapidly pausing threads, swapping memory registers, and managing
3,000 queues than it spends actually doing the MMT math. This is why the graph proves
that performance bottoms out optimally near the physical core count, and worsens if too
many threads are made.
Research Question 2
Did your average time improve compared to LRP01 and LRP02 for everyt?
Whichtis the average time better? Statistically the same? Slower? Why?
Compared to the LRP01, the average time improved massively for allt >1 because
the workload was divided across multiple cores. However, compared to LRP02, the
average time in LRP03 was statistically the same or slower for everyt. This happens
because of several reasons. One of which is because of the reduced available hardware. In
LRP03, the instructions says to intentionally reserve 1 core. By restricting our threads
4
to only the remaining cores (in my case 7 because I have 8 cores), I mathematically
gave the program less processing power than LRP02, which utilized 100% of the CPU.
Connected to that is the second reason why it did not improve, OS scheduler restriction
(load balancing). Modern OS are highly optimized to dynamically shift threads to idle
cores. By locking threads to specific cores using affinity, we forced them to wait if their
assigned core became busy, entirely removing the OS dynamic load balancer.
Research Question 3
Discuss in your own words what will happen if instead you divideXas in step
(3) above, still intotsubmatrices, but in a manner where each submatrix is
of sizen/t×n?
The Min-Max Transformation (MMT) formula requires the global minimum and max-
imum of the entire column to normalize the elements. Dividing the matrix into dimensions
ofn/t×nmeans that we are partitioning the matrix horizontally by chunks of rows. If
a thread is assigned a chunk of rows, it only possesses an incomplete segment of each
column. With this, the thread cannot determine the minimum and maximum of that
column to perform the normalization.
To solve this problem, exactly like what I have done in the LRP02 Row Wise code, I
implemented a two-phase approach. The phase 1 finds the local minimum and maximum
for its segment of every column. Then, synchronization happens to gather all the local
extremes to determine the true global extremes for the entire column. Then, Phase 2
happens where the MMT formula is applied to their assigned rows using newly synchro-
nized minimum and maximum. Even though this row partitioning requires a complex
synchronization phase, it is actually faster than the column-partitioning approach. Be-
cause C stores matrices in Row-Major order, assigning threads by rows ensures Spatial
Cache Locality. The CPU prefetcher eliminates memory stall cache misses, making the
row-wise approach faster despite the synchronization overhead.
5
--- END OF CMSC_180_Lab03_Final.pdf ---

--- START OF DelosReyes_cmsc180_lab01.pdf ---
Laboratory Research Problem 01 (LRP01)
CMSC 180: Introduction to Parallel Computing
Charles Andrei P. De los Reyes
CD-3L
February 4, 2026
Research Question 1
What do you think is the complexity of solving the MMT of an×nsquare
matrix X? (hint: CMSC 142)
The complexity of solving the MMT for ann×nsquare matrixXisO(n 2), which
is known as quadratic complexity in CMSC 142. Themmtfunction uses a nested loop
structure consisting of one outer loop and two inner loops. We can visualize the operations
asn×(n+n) = 2n 2. In Big-O notation, constants are dropped, leaving onlyn 2. This
results in a complexity ofO(n 2).
Data Table
Table 1: Runtime and Complexity Data
n Run 1 (s) Run 2 (s) Run 3 (s) Avg Runtime (s) Comp. (n= 100) Comp. (max(n))
100 0.000063 0.000063 0.000063 0.000063 0.000063 0.000172
200 0.000211 0.000211 0.000210 0.000211 0.000252 0.000689
300 0.000508 0.000515 0.000507 0.000510 0.000567 0.001551
400 0.000898 0.000900 0.001055 0.000951 0.001008 0.002757
500 0.001567 0.001529 0.001731 0.001609 0.001575 0.004308
600 0.002497 0.002361 0.002424 0.002427 0.002268 0.006204
700 0.004327 0.003544 0.004091 0.003987 0.003087 0.008445
800 0.005029 0.004774 0.004268 0.004690 0.004032 0.011029
900 0.007491 0.006897 0.007809 0.007399 0.005103 0.013959
1,000 0.012193 0.014034 0.009304 0.011844 0.006300 0.017234
2,000 0.035476 0.035147 0.039507 0.036710 0.025200 0.068935
4,000 0.174574 0.173671 0.176342 0.174862 0.100800 0.275741
8,000 0.800260 0.795991 0.821764 0.806005 0.403200 1.102964
16,000 3.545907 3.918605 3.587825 3.684112 1.612800 4.411851
20,000 6.937712 6.697515 7.045328 6.893518 2.520000 6.893518
Research Question 2
Were you able to run up ton >20,000? If so, can you make it higher to 50,000
or even 100,000? If not, why do you think so and what do you need to do to
make it so?
1
I was able to run up ton= 46,000 in 76.983459 seconds, but I was unable to run
n= 47,000 or higher; the program simply stops when I attempt it. I believe the reason
for this is limited physical RAM capacity. Although my laptop has 16GB of RAM, the
OS and other background processes consume a significant portion of it. Consequently,
allocating such a large contiguous block of memory exceeds my available physical memory.
To runn= 47,000 and above, I would need to upgrade my hardware by installing more
physical RAM.
Graph Analysis
Figure 1: Plot of Matrix Size (n) vs. Average Runtime and Complexity
Research Question 3
Do the lines agree, at least in the form? If not, provide an explanation why
so?
Yes, the lines agree in form. Based on the graph, the Average Runtime curve closely
follows the Complexity (based on max(n)) curve. Both exhibit a quadratic growth pat-
tern. On the other hand, the Complexity (based onn= 100) line grows more slowly and
remains below the measured runtime. Even though the lines grow at different rates, the
overall shape and trend match, confirming that the results are consistent with quadratic
complexity.
Research Question 4
Discuss ways on how we can make it better (lower average runtime) without
using any extra processors or cores (notice that the word “ways” is in plural
form).
2
Two effective ways to lower the average runtime without using any extra processors
are improving memory access patterns and enabling compiler optimizations.
First, we can improve memory access patterns through techniques like Loop Tiling.
Since C stores arrays in row-major order, our current column-based access forces the CPU
to load a chunk of neighboring data but only use a single value from it before jumping to
a distant memory address. This wastes the loaded data and forces the CPU to constantly
fetch new chunks from the slow main RAM. Loop Tiling solves this problem by processing
small blocks that fit entirely in the cache, ensuring we use loaded data before moving on.
Second, we can enable compiler optimization using flags like-O3. These flags allow the
compiler to use Vectorization (SIMD - Single Instruction, Multiple Data). This utilizes
special CPU registers to process multiple floating-point numbers in a single clock cycle,
rather than processing them one by one. This reduces the instruction count and the
execution time without any changes to the code.
3
--- END OF DelosReyes_cmsc180_lab01.pdf ---

