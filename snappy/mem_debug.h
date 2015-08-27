#ifndef MEM_DEBUG_H
#define MEM_DEBUG_H





//void * operator new( unsigned int size, const char *filename, int line );
void * operator new( unsigned long size);
void operator delete( void *ptr );



    #define DEBUG_NEW new(__FILE__, __LINE__)
    #define new DEBUG_NEW



