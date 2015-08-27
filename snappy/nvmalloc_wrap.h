#ifndef NVMALLOC_WRAP_H
#define NVMALLOC_WRAP_H


#include <stdio.h>
#include <stdlib.h>
#include "nv_map.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROC_ID 4015
#define CHUNK_ID 1
#define MAXSIZE 100 * 1024 * 1024

//#define USE_NVMALLOC
//#define ALLOCATE
//#define FASTA_DELETE_ME
//#define USE_PIN

#define USE_BENCHMARKING

#define OUTFILE "fasta_output"
#define REV_INFILE "fasta_output"
#define REV_OUTFILE "revcomp_output"


static struct rqst_struct rqst;
static pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;


//#define OUTFILE "/tmp/ramsud/fasta_output"
//#define REV_INFILE "/tmp/ramsud/fasta_output"
//#define REV_OUTFILE "/tmp/ramsud/revcomp_output"

/*struct rqst_struct {
    size_t bytes;
    const char* var;
    //unique id on how application wants to identify 
    //this chunk;
    int id;
    int pid;

    int ops;
    void *src;
    unsigned long mem;
    unsigned int order_id;

    //volatile flag
    int isVolatile;
    unsigned int mmap_id;
    unsigned long mmap_straddr;
};

struct nvmap_arg_struct{

    unsigned long fd;
    unsigned long offset;
    int chunk_id;
    int proc_id;
    int pflags;
};*/



void npfree(void *ptr);
void *nprealloc(void *, size_t);
void *npmalloc(size_t);
void *npcalloc(size_t n_elements, size_t elem_size);
size_t print_total_stats();

void *pnvmalloc(size_t size, struct rqst_struct *rqst);
void *pnvread(size_t bytes, struct rqst_struct *rqst);
int  pnvcommit(struct rqst_struct *rqst);

void init_pin();

#ifdef __cplusplus
}

/* Wrapper to check for errors */
#define CHECK_ERR(a)                                       \
   if ((a))                                                  \
   {                                                         \
      perror("Error at line\n\t" #a "\nSystem Msg");         \
      exit(1);                                               \
   }
#endif

#endif


