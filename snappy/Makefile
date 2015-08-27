CC=g++
CFLAGS=-c -Wall -g
LDFLAGS= -lpthread -lm -g
#SOURCES=  nv_map.c oswego_malloc.c np_malloc.c red_black_tree.c stack.c misc.c \
			nvmalloc_wrap.c bench-framework.c  benchstringio.c fasta_10k_ref_output.c \
			deltablue.c binarytrees.c spectralnorm.c spellcheck_io.c revcomp_nvram.c \
			heapsort.c raytracer.c procstat.c IOtimer.c pin_mapper.c 

SOURCES= snappy.cc  snappy-c.cc  snappy-sinksource.cc  snappy-stubs-internal.cc snappy-test.cc snappy_unittest.cc

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bench-framework

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cc.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o	
	rm -rf *.a
	rm -rf *.so
	rm -rf *.so.*
	rm -rf bench-framework
	

