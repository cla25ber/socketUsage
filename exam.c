#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "unbQueue.h"

#define MAX_FILENAME_LENGTH 256 //a file's name cannot be longer than 255 characters (value found using pathconf on the LAB-II machine); the last character is for \0 
#define MAX_LINE_LENGTH 1024  //the programme assumes a line in a file is not longer than 1023 characters (the last one is for \0)
#define MAX_OUTPUTSTRING_LENGTH 33280 //the programme assumes a path cannot contain more than 128 directories (pathconf value is 4096, but that is too big), so the maximum length of a path is 128*256=32768 characters; then 511+1 was added to the total to contain the other informations of the output string (n, avg, std_dev) and the final \0 character
#define SPECIAL_STRING "odwbcw_ionwf_*hsoh83hrkwnnoqn__" //random string that determines when there are no more file names in the bounded buffer
#define SOCKET_NAME "c.brn.617214" // c (the first letter of my name, Claudio), brn (an abbreviation of my surname, Bernardoni), 617214 (my UniPi identification number); these informations composed will be the socket's name

//Checks if the input is equal to -1, in that case an error message is printed; also works for EOF which is defined as -1
#define ec_neg1(S,M) \
if ((S)==-1) {perror(M);exit(EXIT_FAILURE);}
//Checks if the input is equal to NULL ((void*)-1), in that case an error message is printed; also works for MAP_FAILED which is defined as (void*)-1
#define ec_null(S,M) \
if ((S)==NULL) {perror(M);exit(EXIT_FAILURE);}
//Checks if the input is not equal to 0, in that case an error message is printed
#define ne_0(S,M) \
if ((S)!=0) {perror(M);exit(EXIT_FAILURE);}
//Checks if errno is not 0, in that case an error message is printed
#define not0_errno(M) \
if ((errno)!=0) {perror(M);exit(EXIT_FAILURE);}

/* WARNING: this programme uses the sqrt function of the math.h library; the sqrt function is implemented using the Newton-Raphson method, this means that if the standard deviation is so small that it is approximated with 0 by underflow, the sqrt function will return nan, because it will use its parameter in a fraction in the denominator to approssimate the value of the square root, thus resulting in nan */

static int skt_created=0; //global variable that is 0 if the socket has not been created yet or has been eliminated, and is 1 if the socket was created

static pid_t kill_pid=-1; //global variable that holds the pid of a process that has to be killed in case of an error, it's -1 if that process has not been created yet or if it has already been terminated

// Struct passed as the arguments to the worker function, it contains a pointer to the shared queue, a pointer to the socket address, and a pointer to the semaphore at which the thread will be waiting the socket's creation
typedef struct thread_arguments {
    queue_t *queue;
    struct sockaddr_un *skt_addr;
    sem_t *skt_built;
} arg_t;

// Kills the process that may cause troubles and unlinks the socket if the programme returns in an unexpected way
// No inputs
// No return value
void exit_handler(void);

// Explores the directory it is given, if it finds a file with the .dat extension it is put in the shared unbounded queue, otherwise if it finds directories it explores them recursevely
// Takes as input the pointer to the unbounded queue, the path of the directory to explore, and the length of the path
// No return value
void master(queue_t*, char*, int);

    // Checks if a file's name has the .dat extension 
    // Takes as input the string to check
    // Returns 0 if it has the .dat extension or a non-0 value if not
    int is_dat(char*);

// Used inside master

// Pops a file's name from the queue and elaborates its informations, which are then formatted and sent to the collector via a socket
// Takes as input a void pointer, which will be casted to an arg_t pointer
// No return value
void *worker(void*);

    // Takes all the numbers in a file and counts them, makes their average and standard deviation; these informations are then formatted in a string
    // Takes as input the path of the file to get the data from, and the string in which all the informations will be formatted
    // No return value
    void get_informations(const char*, char*);

        // Checks if a line is composed solely of white-space characters
        // Takes as input the string to check
        // Returns 1 if the string is composed only of white-space characters, otherwise 0
        int is_blank(char*);

        // Checks if a string contains only a number (both integers and decimal numbers are valid)
        // Takes as input the string to check
        // Returns 1 if the string contains only a number, otherwise 0
        int is_number(char*);

    // Used inside get_informations

// Used inside worker

// Creates a server socket to print the data collected by worker threads
// Takes as input the socket address, the semaphore at which the workers will be waiting for the socket to be created, and the number of worker threads
// No return value
void collector(struct sockaddr_un*, sem_t*, int);

    // Counts how many file descriptors are set, up to the max file descriptor
    // Takes as input the file descriptor set to check, and the max file descriptor
    // Returns the number of file descriptors set
    int count_set(fd_set, int);

    // Adjusts the max file descriptor based on which file descriptors are set
    // Takes as input the set to adjust and the pointer to the max file descriptor
    // No return value
    void update_max(fd_set, int*);

// Used inside collector

// Main function
int main(int argc, char *argv[]) {
    //Checking if the input is correct
    if (argc<3) {
        fprintf(stderr, "%s: Not enough arguments error\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct stat inputdir;
    ec_neg1(stat(argv[1], &inputdir), "Input metadata collection error");
    if ((S_ISDIR(inputdir.st_mode))!=1) {
        fprintf(stderr, "Input not a directory error\n");
        exit(EXIT_FAILURE);
	}
    int W=atoi(argv[2]); //there's no need to check if the input for W is a number because when atoi fails it returns 0 so the programme will stop anyway
    if (W<1) {
        fprintf(stderr, "Not enough workers error\n");
        exit(EXIT_FAILURE);
    }
    if (W>SOMAXCONN) {
        fprintf(stderr, "Changing number of workers from %d to %d because of maximum queue length specified by listen\n", W, SOMAXCONN);
        W=SOMAXCONN;
    }
    //Setting exit handler
    ec_neg1(atexit(exit_handler), "Setting exit handler error");
    //Creating socket address
    struct sockaddr_un skt_addr; //socket address
    skt_addr.sun_family=AF_UNIX;
    strncpy(skt_addr.sun_path, SOCKET_NAME, MAX_FILENAME_LENGTH);
    //Creating semaphore
    sem_t* skt_built=(sem_t*)mmap(NULL, sizeof(sem_t), (PROT_READ | PROT_WRITE), (MAP_SHARED | MAP_ANONYMOUS), -1, 0);
	ec_null(skt_built, "Mapping memory for semaphore error"); //if unsuccessfull mmap returns MAP_FAILED which is defined as (void*)-1
    ec_neg1(sem_init(skt_built, 1, 0), "Initializing semaphore error");
    //Creating collector process
    pid_t col_pid=fork(); //collector pid
    ec_neg1(col_pid, "Creating collector process error");
    if (col_pid==0) {
        collector(&skt_addr, skt_built, W);
        return 0;
    }
    kill_pid=col_pid;
    //Creating queue
    queue_t *q=initQueue();
    ec_null(q, "Queue initialization error");
    //Preparing input for threads
    arg_t *args=(arg_t*)malloc(sizeof(arg_t));
    ec_null(args, "Allocating arguments for thread argument error");
    args->queue=q;
    args->skt_addr=&(skt_addr);
    args->skt_built=skt_built;
    //Creating worker threads
    pthread_t tid[W];
    for (int i=0; i<W; i++) {
        ne_0(pthread_create(&(tid[i]), NULL, worker, args), "Creating thread error");
    }
    //Master work
    master(q, argv[1], strlen(argv[1])+1);
    ec_neg1(push(q, SPECIAL_STRING), "Pushing element to queue error"); //after having finished putting all the files in the queue, the SPECIAL_STRING is pushed to indicate the end of the files to the threads
    //Joining worker threads
    for (int i=0; i<W; i++) {
        ne_0(pthread_join(tid[i], NULL), "Joining thread error");
    }
    //Eliminating thread input
    free(args);
    //Dismantling queue
    delQueue(q);
    //Waiting for collector process
    int status=0;
    ec_neg1(waitpid(col_pid, &status, 0), "Waiting for collector process error");
    kill_pid=-1;
    if (status==EXIT_FAILURE) {
        fprintf(stderr, "Unexpected exit status from collector process");
        exit(EXIT_FAILURE);
    }
    //Destroying and unmapping semaphore
    ec_neg1(sem_destroy(skt_built), "Destroying semaphore error");
    ec_neg1(munmap(skt_built, sizeof(sem_t)), "Unmapping semaphore error");
    //Unlinking socket
    ec_neg1(unlink(SOCKET_NAME), "Unlinking file error");
    skt_created=0;
    //Success return value
    return 0;
}

void exit_handler(void) {
    if (kill_pid>0) {
        if (kill(kill_pid, SIGTERM)==-1) {
            perror("Killing process error");
        }
    }
    if (skt_created==1) {
        if(unlink(SOCKET_NAME)==-1) {
            perror("Unlinking socket error");
        }
    }
    return;
}

void master(queue_t *q, char *path, int size) {
    DIR *current=opendir(path);
    ec_null(current, "Opening directory error");
	strncat(path, "/", 2);
	struct dirent* file;
    errno=0;
	while ((file=readdir(current))!=NULL) {
        not0_errno("Reading directory error");
		if ((strncmp(file->d_name, ".", 2)!=0) && (strncmp(file->d_name, "..", 3)!=0)) { //the . and .. directories are obviously avoided as they would make the master function end up in an infinite loop
            char new_path[size+MAX_FILENAME_LENGTH];
            memset(new_path, 0, size+MAX_FILENAME_LENGTH);
			strncpy(new_path, path, size);
			strncat(new_path, file->d_name, MAX_FILENAME_LENGTH);
            int len=strlen(new_path);
			struct stat statbuf;
			ec_neg1(stat(new_path, &statbuf), "File metadata collection error");
			if ((S_ISDIR(statbuf.st_mode))!=0) {    //if it's a directory
                master(q, new_path, len+1);         //it is explored recursevely
			} else {
				if ((S_ISREG(statbuf.st_mode))!=0) {                                    //if it's a file
					if ((is_dat(file->d_name))==0) {                                    //with the .dat extension
                        char *file_path=(char*)malloc(sizeof(char)*(len+1));
                        ec_null(file_path, "Allocating memory for file's name error");
                        strncpy(file_path, new_path, len+1);
                        ec_neg1(push(q, file_path), "Pushing element to queue error");  //it is added to the queue
                    }
				}
			}
		}
        errno=0;
	}
    ec_neg1(closedir(current), "Closing directory error");
    return;
}

int is_dat(char name[MAX_FILENAME_LENGTH]) {
	int l=strlen(name);
	if (l>4) { //the file must be at least 5 characters long, 4 for the extension (.dat) and at least 1 character for the name of the file
		char* cut=&(name[l-4]);
		return strncmp(cut, ".dat", 5); //strncmp returns 0 if the two string up to the n-th character are the same, otherwise returns 1 or -1
	}
	return -1; //if it is not long enough -1 is returned
}

void *worker(void* args) {
    queue_t *q=((arg_t*)args)->queue;
    struct sockaddr_un *skt_addr=((arg_t*)args)->skt_addr;
    sem_t* skt_built=((arg_t*)args)->skt_built;
    ec_neg1(sem_wait(skt_built), "Waiting for semaphore error");
    skt_created=1; //the socket has been created
    int skt_fd=socket(AF_UNIX, SOCK_STREAM, 0);
    ec_neg1(skt_fd, "Creating client socket error");
    ec_neg1(connect(skt_fd, (struct sockaddr*)skt_addr, sizeof(*skt_addr)), "Connecting client error");
	while (1) { //always true, the cycle breaks only when SPECIAL_STRING is found
        char *filename=(char*)pop(q);
        ec_null(filename, "Popping element from queue error");
        int l=strlen(filename);
		if (strncmp(filename, SPECIAL_STRING, l+1)==0) { //if the child process got SPECIAL_STRING, it means that no more files' name are in the shared queue 
            push(q, SPECIAL_STRING);
			break;
		}
		char final_string[MAX_OUTPUTSTRING_LENGTH]; //all the file's informations will be formatted in this string
        memset(final_string, 0, MAX_OUTPUTSTRING_LENGTH);
		get_informations(filename, final_string);
		ec_neg1(write(skt_fd, final_string, MAX_OUTPUTSTRING_LENGTH), "Writing to server error");
        free(filename);
	}
    ec_neg1(close(skt_fd), "Closing client socket error");
    return NULL;
}

void get_informations(const char *f_path, char final_string[MAX_OUTPUTSTRING_LENGTH]) {
	FILE *f=fopen(f_path, "r");
    ec_null(f, "Opening file error");
	double sum=0.0;
	double sum_squared=0.0;
	int counter=0;
	char line[MAX_LINE_LENGTH];
	while (fgets(line, MAX_LINE_LENGTH-1, f)!=NULL) {
		if (is_blank(line)==0) { //the atof function return 0.0 in case of a blank line, this makes it difficult to differentiate between a blank line and a 0, so the progamme needs to know in which case it is, because then a blank line would update the counter
			if (is_number(line)==1) { //the programme assumes, as it is specified in the project's requirements, that there is only one number per line
                double x=atof(line);
			    sum+=x;
			    sum_squared+=(x*x);
			    counter+=1;
            }
		}
	}
    ec_neg1(fclose(f), "Closing file error"); //if unsuccessfull fclose returns EOF which is defined as -1
	if (counter>0) { //this condition consider the case of a blank file, in that case the else branch sets everything to 0; this condition is necessary otherwise dividing by counter (which would be 0) would result in nan
		double average=sum/(double)counter;
		double std_dev=sqrt((sum_squared/(double)counter)-(average*average)); //by calculating the standard deviation in this way, the programme doesn't need another cycle to calculate the squared sum of the numbers minus the average
		sprintf(final_string, "%d\t%.2f\t%.2f\t%s", counter, average, std_dev, f_path);
	} else {
		sprintf(final_string, "0\t0\t0\t%s", f_path);
	}
	return;
}

int is_blank(char line[MAX_LINE_LENGTH]) {
	int i=0;
	while ((line[i]!='\0') && (i<MAX_LINE_LENGTH)) {
		if ((isspace(line[i]))==0) {
			return 0;
		}
		i+=1;
	}
	return 1;
}

int is_number(char line[MAX_LINE_LENGTH]) {
    int digits=0;   //0 if no digits have been encountered, otherwise 1
    int neg=0;      //0 if no - sign has been encountered, otherwise 1
    int dot=0;      //0 if no . has been encountered, othrwise 1
    int i=0;
	while ((line[i]!='\0') && (i<MAX_LINE_LENGTH)) {
        if (isspace(line[i])!=0) {          //if a character is a white-space character
            if (digits==1) {                    //and some digits have been encountered
                return 1;                           //then the number has ended and 1 is returned
            } else {                            //otherwise if the character is a white-space character but no digit has been ecountered
                if (dot==1) {                       //if a dot has been encountered
                    return 0;                           //the we ecountered a lonely . which is not a number and 0 is returned
                }                                   //
            }                                   //
        } else {                            //if the character is a non white-space character
            if (line[i]=='-') {                 //but it is a - sign
                if (neg==0) {                       //if the - sign has not been encountered yet
                    neg=1;                              //then the neg variable is set
                } else {                            //otherwise it has been encountered twice
                    return 0;                           //so it cannot be a number and 0 is returned
                }                                   //
            } else {                            //if the character is not a white-space character and not a - sign
                if (isdigit(line[i])!=0) {          //but it is a digit
                    digits=1;                           //then the digits variable is set
                } else {                            //otherwise if it isn't a white-space character, a - sign and a digit
		            if (line[i]=='.') {                 //if it is a .
                        if (dot==0) {                       //and a . has not been encountered yet
                            dot=1;                              //the dot variable is set
                        } else {                            //otherwise if it has been encountered
                            return 0;                           //0 is returned because a number cannot contain two .
                        }                                   //
                    } else {                            //if it isn't a white-space character, a - sign, a digit or a .
                        return 0;                           //then it can't possibly be a number and 0 is returned
                    }                                   //
                }                                   //
            }                                   //
        }                                   //
        i++;
	}
    return 1;                               //if the line ended while respecting all of the conditions, then it's a number and 1 is returned
}

void collector(struct sockaddr_un *skt_addr, sem_t *skt_built, int W) {
    int skt_fd=socket(AF_UNIX, SOCK_STREAM, 0); //socket file descriptor
    ec_neg1(skt_fd, "Creating server socket error");
    ec_neg1(bind(skt_fd, (struct sockaddr*)skt_addr, sizeof(*skt_addr)), "Binding socket error");
    ec_neg1(listen(skt_fd, W), "Marking socket error");
    skt_created=1;
    kill_pid=getppid();
    for (int i=0; i<W; i++) {
        ec_neg1(sem_post(skt_built), "Incrementing semaphore error"); //in case sem_post fails, the exit_handler sends a signal to the parent process, which will make sem_wait fail and make the parent process exit too
    }
    kill_pid=-1;
    skt_created=0;
    printf("n\tavg\tstd\tfile\n--------------------------------------------------------------------------\n");
    int fd_max=skt_fd;  //fd_max is the max file descriptor
    int arrived=0;      //arrived counts how many threads have been connected to the server
    fd_set set, rd_set; //set is the current file descriptor set & rd_set is the reading file descriptor set
    FD_ZERO(&set);
    FD_SET(skt_fd, &set);
    while (arrived<W || count_set(set, fd_max)>1) { //the iteration stops when all threads have been connected and only the socket's file descriptor is left in the set
        rd_set=set;
        ec_neg1(select(fd_max+1, &rd_set, NULL, NULL, NULL), "Waiting for I/O error");
        for (int fd=0; fd<fd_max+1; fd++) {
            if (FD_ISSET(fd, &rd_set)==1) {
                if (fd==skt_fd) {
                    int fd_client=accept(skt_fd, NULL, 0); //client file descriptor
                    ec_neg1(fd_client, "Accepting new connection error");
                    FD_SET(fd_client, &set);
                    if (fd_client>fd_max) {
                        fd_max=fd_client;
                    }
                    arrived++;
                } else {
                    char output_string[MAX_OUTPUTSTRING_LENGTH];
                    int n=read(fd, output_string, MAX_OUTPUTSTRING_LENGTH);
                    ec_neg1(n, "Reading from socket error");
                    if (n!=0) {
                        printf("%s\n", output_string);
                    } else { //if 0 bytes are read it means that the client has disconnected
                        FD_CLR(fd, &set);
                        if (fd==fd_max) {
                            update_max(set, &fd_max);
                        }
                        ec_neg1(close(fd), "Closing client socket error");
                    }
                }
            }
        }
    }
    ec_neg1(close(skt_fd), "Closing server socket error");
    return;
}

int count_set(fd_set set, int fd_max) {
    int count=0;
    for (int fd=0; fd<fd_max+1; fd++) {
        if (FD_ISSET(fd, &set)==1) {
            count++;
        }
    }
    return count;
}

void update_max(fd_set set, int *fd_max) {
    while (FD_ISSET(*fd_max, &set)==0) {
        (*fd_max)--;
    }
    return;
}