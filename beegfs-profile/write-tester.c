#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

char* rootdir;
int numthreads;

struct targs {
    int num;
    size_t size;
};

long elapsed_micro(struct timespec* start, struct timespec* end) {
  return ((end->tv_sec - start->tv_sec) * 1000 * 1000) +
    ((end->tv_nsec - start->tv_nsec) / 1000);
}

int write_all(int fd, const char* buf, size_t count) {
    size_t written = 0;
    size_t ret = 0;
    do {
        if ((ret = write(fd, buf + written, count - written)) < 0) {
            printf("write error\n");
            return -1;
        }
        written += ret;
    } while (written < count);
    
    return 0;
}

void *writer(void *arg) {
    struct timespec writestart, writeend, fsyncstart, fsyncend;
    struct targs* a = (struct targs*)arg;
    int reps = 100;
    
    int fd;
    char filename[200];
    sprintf(filename, "%s/thread-%d.dat", rootdir, a->num);

    if ((fd = open(filename, O_CREAT | O_RDWR, S_IRWXU)) < 0) {
      printf("thread %d failure to open file\n", a->num);
      exit(1);
    }

    char* buf = malloc(sizeof(char) * a->size * reps);
    for (int i = 0; i < reps; i++) {
	clock_gettime(CLOCK_MONOTONIC, &writestart);
        if (write_all(fd, buf + (i*a->size), a->size) < 0) {
            exit(1);
        }
	clock_gettime(CLOCK_MONOTONIC, &writeend);

	clock_gettime(CLOCK_MONOTONIC, &fsyncstart);	
	fsync(fd);
	clock_gettime(CLOCK_MONOTONIC, &fsyncend);

	printf("write\t%d\t%ld\t%ld\tfsync\t%ld\n", i, a->size,
	       elapsed_micro(&writestart, &writeend),
	       elapsed_micro(&fsyncstart, &fsyncend));
    }

    free(buf);
    free(arg);
    return NULL;
}
    
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("usage: ./writetest <rootdir> <numthreads> <size>\n");
        exit(1);
    }

    rootdir = strdup(argv[1]);
    numthreads = atoi(argv[2]);
    size_t size = atol(argv[3]);

    printf("rootdir: %s\tthrds: %d\tsize: %ld\n",
           rootdir, numthreads, size);

    pthread_t* thrds = (pthread_t*)malloc(numthreads * sizeof(pthread_t));
    for (int i = 0; i < numthreads; i++) {
        struct targs* a = malloc(sizeof(struct targs));
        a->num = i;
        a->size = size;
        if (pthread_create(&thrds[i], NULL, writer, a) != 0) {
            free(a);
            printf("thread create error\n");
            exit(1);
        }
    }

    for (int i = 0; i < numthreads; i++) {
        pthread_join(thrds[i], NULL);
    }
    
    return 0;    
}
