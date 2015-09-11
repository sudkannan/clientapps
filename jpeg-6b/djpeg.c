/*
 * djpeg.c
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains a command-line user interface for the JPEG decompressor.
 * It should work on any system with Unix- or MS-DOS-style command lines.
 *
 * Two different command line styles are permitted, depending on the
 * compile-time switch TWO_FILE_COMMANDLINE:
 *	djpeg [options]  inputfile outputfile
 *	djpeg [options]  [inputfile]
 * In the second style, output is always to standard output, which you'd
 * normally redirect to a file or pipe to some other program.  Input is
 * either from a named file or from standard input (typically redirected).
 * The second style is convenient on Unix but is unhelpful on systems that
 * don't support pipes.  Also, you MUST use the first style if your system
 * doesn't do binary I/O to stdin/stdout.
 * To simplify script writing, the "-outfile" switch is provided.  The syntax
 *	djpeg [options]  -outfile outputfile  inputfile
 * works regardless of which command line style is used.
 */

#include "cdjpeg.h"		/* Common decls for cjpeg/djpeg applications */
#include "jversion.h"		/* for version message */
#include <assert.h>

#include <ctype.h>		/* to declare isprint() */

#ifdef USE_CCOMMAND		/* command-line reader for Macintosh */
#ifdef __MWERKS__
#include <SIOUX.h>              /* Metrowerks needs this */
#include <console.h>		/* ... and this */
#endif
#ifdef THINK_C
#include <console.h>		/* Think declares it here */
#endif
#endif

#define USE_DIRECTORY

#ifdef USE_DIRECTORY
#include <sys/types.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#endif

#include <nv_map.h>
#include <c_io.h>

#ifdef USE_NVM
#include <nv_map.h>
#include <c_io.h>
#endif



#define USE_NVML
#ifdef USE_NVML
#include <libpmem.h>
#include <libpmemobj.h>
#include "libpmemlog.h"

#define COMPLEXLOG
//#define SIMPLELOG

#define	LAYOUT_NAME "pmsnappy"
#define PMSNAPPY_POOL_SIZE 1024*1024*1024
#define FILEPATH "/tmp/ramdisk/test"


#ifdef COMPLEXLOG
POBJ_LAYOUT_BEGIN(obj_pmemlog_macros);
POBJ_LAYOUT_ROOT(obj_pmemlog_macros, struct base);
POBJ_LAYOUT_TOID(obj_pmemlog_macros, struct log);
POBJ_LAYOUT_END(obj_pmemlog_macros);

PMEMlogpool *plp1;



/* log entry header */
struct log_hdr {
	TOID(struct log) next;	/* object ID of the next log buffer */
	size_t size;			/* size of this log buffer */
};

/* struct log stores the entire log entry */
struct log {
	struct log_hdr hdr;		/* entry header */
	char data[];			/* log entry data */
};

/* struct base keeps track of the beginning of the log list */
struct base {
	TOID(struct log) head;	/* object ID of the first log buffer */
	TOID(struct log) tail;	/* object ID of the last log buffer */
	PMEMrwlock rwlock;		/* lock covering entire log */
	size_t bytes_written;		/* number of bytes stored in the pool */
};

/*
 * pmemlog_open -- pool open wrapper
 */
PMEMlogpool *
pmemlog_open(const char *path)
{

	return (PMEMlogpool *)pmemobj_open(path,
				POBJ_LAYOUT_NAME(obj_pmemlog_macros));
}

/*
 * pmemlog_create -- pool create wrapper
 */
PMEMlogpool *
pmemlog_create_jpeg()
{
	return (PMEMlogpool *)pmemobj_create(FILEPATH,
				POBJ_LAYOUT_NAME(obj_pmemlog_macros),
				PMSNAPPY_POOL_SIZE, 0666);
}

/*
 * pool_close -- pool close wrapper
 */
void
pmemlog_close(PMEMlogpool *plp)
{
	pmemobj_close((PMEMobjpool *)plp);
}

/*
 * pmemlog_nbyte -- not available in this implementation
 */
size_t
pmemlog_nbyte(PMEMlogpool *plp)
{
	/* N/A */
	return 0;
}



/*
 * pmemlog_append -- add data to a log memory pool
 */
size_t
pmemlog_append_jpeg(const char* input, size_t count)
/*, CompressorType comp,
	char* compressed, int compressed_is_preallocated, bool iscompress)*/
{
	PMEMobjpool *pop = (PMEMobjpool *)plp1;
	int retval = 0;
	TOID(struct base) bp;

	bp = POBJ_ROOT(pop, struct base);

	/* begin a transaction, also acquiring the write lock for the log */
	TX_BEGIN_LOCK(pop, TX_LOCK_RWLOCK, &D_RW(bp)->rwlock, TX_LOCK_NONE) {

		/* allocate the new node to be inserted */
		TOID(struct log) logp;
		logp = TX_ALLOC(struct log, count + sizeof (struct log_hdr));

		//fprintf(stdout,"appending to pmem log %u\n", count);
		D_RW(logp)->hdr.size = count;
		memcpy(D_RW(logp)->data, input, count);
		D_RW(logp)->hdr.next = TOID_NULL(struct log);

		/* add the modified root object to the undo log */
		TX_ADD(bp);
		if (TOID_IS_NULL(D_RO(bp)->tail)) {
			/* update head */
			D_RW(bp)->head = logp;
		} else {
			/* add the modified tail entry to the undo log */
			TX_ADD(D_RW(bp)->tail);
			D_RW(D_RW(bp)->tail)->hdr.next = logp;
		}

		D_RW(bp)->tail = logp; /* update tail */
		D_RW(bp)->bytes_written += count;
	} TX_ONABORT {
		retval = -1;
	} TX_END

	return count;
}

/*
 * pmemlog_rewind -- discard all data, resetting a log memory pool to empty
 */
void
pmemlog_rewind(PMEMlogpool *plp)
{
	PMEMobjpool *pop = (PMEMobjpool *)plp;
	TOID(struct base) bp;
	bp = POBJ_ROOT(pop, struct base);

	/* begin a transaction, also acquiring the write lock for the log */
	TX_BEGIN_LOCK(pop, TX_LOCK_RWLOCK, &D_RW(bp)->rwlock, TX_LOCK_NONE) {
		/* add the root object to the undo log */
		TX_ADD(bp);
		while (!TOID_IS_NULL(D_RO(bp)->head)) {
			TOID(struct log) nextp;
			nextp = D_RW(D_RW(bp)->head)->hdr.next;
			TX_FREE(D_RW(bp)->head);
			D_RW(bp)->head = nextp;
		}

		D_RW(bp)->head = TOID_NULL(struct log);
		D_RW(bp)->tail = TOID_NULL(struct log);
	} TX_END
}

#else

#ifdef SIMPLELOG
struct snappyfile{
	void* data;
};

/* types of allocations */
enum types {
	LOG_TYPE,
	LOG_HDR_TYPE,
	BASE_TYPE,
	MAX_TYPES
};


/* log entry header */
struct log_hdr {
	PMEMoid next;		/* object ID of the next log buffer */
	size_t size;		/* size of this log buffer */
};

/* struct log stores the entire log entry */
struct log {
	struct log_hdr hdr;	/* entry header */
	char data[];		/* log entry data */
};

/* struct base keeps track of the beginning of the log list */
struct base {
	PMEMoid head;		/* object ID of the first log buffer */
	PMEMoid tail;		/* object ID of the last log buffer */
	PMEMrwlock rwlock;	/* lock covering entire log */
	size_t bytes_written;	/* number of bytes stored in the pool */
};

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(pmsnappy);
POBJ_LAYOUT_ROOT(pmsnappy, struct snappyfile);
POBJ_LAYOUT_END(pmsnappy);
PMEMlogpool *plp1;

PMEMlogpool *pmemlog_create()
{

	const char *path = FILEPATH;
	PMEMlogpool *pop;
		pop = NULL;

	srand(time(NULL));

	if (access(path, F_OK) != 0) {
		if ((pop = (PMEMlogpool *)pmemobj_create(path, LAYOUT_NAME,
				PMSNAPPY_POOL_SIZE, 0666)) == NULL) {
			printf("failed to create pool\n");
			return NULL;
		}
		/* create the player and initialize with a constructor */
		//POBJ_NEW(pop, NULL, struct player, create_player, NULL);
	} else {
		if ((pop = (PMEMlogpool *)pmemobj_open(path, LAYOUT_NAME)) == NULL) {
			printf("failed to open pool\n");
			return NULL;
		}
	}
	return pop;
}


/*
 * pmemlog_append -- add data to a log memory pool
 */


//size_t
//pmemlog_append(const char* input, size_t count, CompressorType comp,
	//	char* compressed, bool compressed_is_preallocated)
size_t
pmemlog_append(const char* input, size_t count) /*, CompressorType comp,
		char* compressed, int compressed_is_preallocated, bool iscompress)*/

{
	PMEMobjpool *pop = (PMEMobjpool *)plp1;
	PMEMoid baseoid = pmemobj_root(pop, sizeof (struct base));
	struct base *bp = (struct base*)pmemobj_direct(baseoid);
	size_t outsize=0;

	/* set the return point */
	jmp_buf env;
	if (setjmp(env)) {
		/* end the transaction */
		pmemobj_tx_end();
		return 1;
	}

	/* begin a transaction, also acquiring the write lock for the log */
	pmemobj_tx_begin(pop, env, TX_LOCK_RWLOCK, &bp->rwlock, TX_LOCK_NONE);

	/* allocate the new node to be inserted */
	PMEMoid log = (PMEMoid)pmemobj_tx_alloc(count + sizeof (struct log_hdr), LOG_TYPE);

	struct log *logp = (struct log *)pmemobj_direct(log);
	logp->hdr.size = count;

	outsize = Compress_NVRAM(input, count, SNAPPY, compressed, false);
	memcpy(logp->data, compressed, count);

	logp->hdr.next = OID_NULL;

	/* add the modified root object to the undo log */
	pmemobj_tx_add_range(baseoid, 0, sizeof (struct base));
	if (bp->tail.off == 0) {
		/* update head */
		bp->head = log;
	} else {
		/* add the modified tail entry to the undo log */
		pmemobj_tx_add_range(bp->tail, 0, sizeof (struct log));
		((struct log *)pmemobj_direct(bp->tail))->hdr.next = log;
	}

	bp->tail = log; /* update tail */
	bp->bytes_written += count;
	pmemobj_tx_commit();
	pmemobj_tx_end();

	//fprintf(stdout,"outsize %lu, count %lu \n",outsize, count);
	return outsize;
}


/*
 * pmemlog_rewind -- discard all data, resetting a log memory pool to empty
 */
void
pmemlog_rewind()
{
	PMEMobjpool *pop = (PMEMobjpool *)plp1;
	PMEMoid baseoid = pmemobj_root(pop, sizeof (struct base));
	struct base *bp = (struct base*)pmemobj_direct(baseoid);

	/* set the return point */
	jmp_buf env;
	if (setjmp(env)) {
		/* end the transaction */
		pmemobj_tx_end();
		return;
	}

	/* begin a transaction, also acquiring the write lock for the log */
	pmemobj_tx_begin(pop, env, TX_LOCK_RWLOCK, &bp->rwlock, TX_LOCK_NONE);
	/* add the root object to the undo log */
	pmemobj_tx_add_range(baseoid, 0, sizeof (struct base));

	/* free all log nodes */
	while (bp->head.off != 0) {
		PMEMoid nextoid =
			((struct log *)pmemobj_direct(bp->head))->hdr.next;
		pmemobj_tx_free(bp->head);
		bp->head = nextoid;
	}

	bp->head = OID_NULL;
	bp->tail = OID_NULL;

	pmemobj_tx_commit();
	pmemobj_tx_end();
}
#endif
#endif
#endif



/* Create the add-on message string table. */

#define JMESSAGE(code,string)	string ,

static const char * const cdjpeg_message_table[] = {
#include "cderror.h"
		NULL
};


/*
 * This list defines the known output image formats
 * (not all of which need be supported by a given version).
 * You can change the default output format by defining DEFAULT_FMT;
 * indeed, you had better do so if you undefine PPM_SUPPORTED.
 */

typedef enum {
	FMT_BMP,		/* BMP format (Windows flavor) */
	FMT_GIF,		/* GIF format */
	FMT_OS2,		/* BMP format (OS/2 flavor) */
	FMT_PPM,		/* PPM/PGM (PBMPLUS formats) */
	FMT_RLE,		/* RLE format */
	FMT_TARGA,		/* Targa format */
	FMT_TIFF		/* TIFF format */
} IMAGE_FORMATS;

#ifndef DEFAULT_FMT		/* so can override from CFLAGS in Makefile */
#define DEFAULT_FMT	FMT_PPM
#endif

static IMAGE_FORMATS requested_fmt;


/*
 * Argument-parsing code.
 * The switch parser is designed to be useful with DOS-style command line
 * syntax, ie, intermixed switches and file names, where only the switches
 * to the left of a given file name affect processing of that file.
 * The main program in this file doesn't actually use this capability...
 */


static const char * progname;	/* program name for error messages */
static char * outfilename;	/* for -outfile switch */


LOCAL(void)
usage (void)
/* complain about bad command line */
{
	fprintf(stderr, "usage: %s [switches] ", progname);
#ifdef TWO_FILE_COMMANDLINE
	fprintf(stderr, "inputfile outputfile\n");
#else
	fprintf(stderr, "[inputfile]\n");
#endif

	fprintf(stderr, "Switches (names may be abbreviated):\n");
	fprintf(stderr, "  -colors N      Reduce image to no more than N colors\n");
	fprintf(stderr, "  -fast          Fast, low-quality processing\n");
	fprintf(stderr, "  -grayscale     Force grayscale output\n");
#ifdef IDCT_SCALING_SUPPORTED
	fprintf(stderr, "  -scale M/N     Scale output image by fraction M/N, eg, 1/8\n");
#endif
#ifdef BMP_SUPPORTED
	fprintf(stderr, "  -bmp           Select BMP output format (Windows style)%s\n",
			(DEFAULT_FMT == FMT_BMP ? " (default)" : ""));
#endif
#ifdef GIF_SUPPORTED
	fprintf(stderr, "  -gif           Select GIF output format%s\n",
			(DEFAULT_FMT == FMT_GIF ? " (default)" : ""));
#endif
#ifdef BMP_SUPPORTED
	fprintf(stderr, "  -os2           Select BMP output format (OS/2 style)%s\n",
			(DEFAULT_FMT == FMT_OS2 ? " (default)" : ""));
#endif
#ifdef PPM_SUPPORTED
	fprintf(stderr, "  -pnm           Select PBMPLUS (PPM/PGM) output format%s\n",
			(DEFAULT_FMT == FMT_PPM ? " (default)" : ""));
#endif
#ifdef RLE_SUPPORTED
	fprintf(stderr, "  -rle           Select Utah RLE output format%s\n",
			(DEFAULT_FMT == FMT_RLE ? " (default)" : ""));
#endif
#ifdef TARGA_SUPPORTED
	fprintf(stderr, "  -targa         Select Targa output format%s\n",
			(DEFAULT_FMT == FMT_TARGA ? " (default)" : ""));
#endif
	fprintf(stderr, "Switches for advanced users:\n");
#ifdef DCT_ISLOW_SUPPORTED
	fprintf(stderr, "  -dct int       Use integer DCT method%s\n",
			(JDCT_DEFAULT == JDCT_ISLOW ? " (default)" : ""));
#endif
#ifdef DCT_IFAST_SUPPORTED
	fprintf(stderr, "  -dct fast      Use fast integer DCT (less accurate)%s\n",
			(JDCT_DEFAULT == JDCT_IFAST ? " (default)" : ""));
#endif
#ifdef DCT_FLOAT_SUPPORTED
	fprintf(stderr, "  -dct float     Use floating-point DCT method%s\n",
			(JDCT_DEFAULT == JDCT_FLOAT ? " (default)" : ""));
#endif
	fprintf(stderr, "  -dither fs     Use F-S dithering (default)\n");
	fprintf(stderr, "  -dither none   Don't use dithering in quantization\n");
	fprintf(stderr, "  -dither ordered  Use ordered dither (medium speed, quality)\n");
#ifdef QUANT_2PASS_SUPPORTED
	fprintf(stderr, "  -map FILE      Map to colors used in named image file\n");
#endif
	fprintf(stderr, "  -nosmooth      Don't use high-quality upsampling\n");
#ifdef QUANT_1PASS_SUPPORTED
	fprintf(stderr, "  -onepass       Use 1-pass quantization (fast, low quality)\n");
#endif
	fprintf(stderr, "  -maxmemory N   Maximum memory to use (in kbytes)\n");
	fprintf(stderr, "  -outfile name  Specify name for output file\n");
	fprintf(stderr, "  -verbose  or  -debug   Emit debug output\n");
	exit(EXIT_FAILURE);
}


LOCAL(int)
parse_switches (j_decompress_ptr cinfo, int argc, char **argv,
		int last_file_arg_seen, boolean for_real)
		/* Parse optional switches.
		 * Returns argv[] index of first file-name argument (== argc if none).
		 * Any file names with indexes <= last_file_arg_seen are ignored;
		 * they have presumably been processed in a previous iteration.
		 * (Pass 0 for last_file_arg_seen on the first or only iteration.)
		 * for_real is FALSE on the first (dummy) pass; we may skip any expensive
		 * processing.
		 */
		{
	int argn;
	char * arg;

	/* Set up default JPEG parameters. */
	requested_fmt = DEFAULT_FMT;	/* set default output file format */
	outfilename = NULL;
	cinfo->err->trace_level = 0;

	/* Scan command line options, adjust parameters */

	for (argn = 1; argn < argc; argn++) {
		arg = argv[argn];
		if (*arg != '-') {
			/* Not a switch, must be a file name argument */
			if (argn <= last_file_arg_seen) {
				outfilename = NULL;	/* -outfile applies to just one input file */
				continue;		/* ignore this name if previously processed */
			}
			break;			/* else done parsing switches */
		}
		arg++;			/* advance past switch marker character */

		if (keymatch(arg, "bmp", 1)) {
			/* BMP output format. */
			requested_fmt = FMT_BMP;

		} else if (keymatch(arg, "colors", 1) || keymatch(arg, "colours", 1) ||
				keymatch(arg, "quantize", 1) || keymatch(arg, "quantise", 1)) {
			/* Do color quantization. */
			int val;

			if (++argn >= argc)	/* advance to next argument */
				usage();
			if (sscanf(argv[argn], "%d", &val) != 1)
				usage();
			cinfo->desired_number_of_colors = val;
			cinfo->quantize_colors = TRUE;

		} else if (keymatch(arg, "dct", 2)) {
			/* Select IDCT algorithm. */
			if (++argn >= argc)	/* advance to next argument */
				usage();
			if (keymatch(argv[argn], "int", 1)) {
				cinfo->dct_method = JDCT_ISLOW;
			} else if (keymatch(argv[argn], "fast", 2)) {
				cinfo->dct_method = JDCT_IFAST;
			} else if (keymatch(argv[argn], "float", 2)) {
				cinfo->dct_method = JDCT_FLOAT;
			} else
				usage();

		} else if (keymatch(arg, "dither", 2)) {
			/* Select dithering algorithm. */
			if (++argn >= argc)	/* advance to next argument */
				usage();
			if (keymatch(argv[argn], "fs", 2)) {
				cinfo->dither_mode = JDITHER_FS;
			} else if (keymatch(argv[argn], "none", 2)) {
				cinfo->dither_mode = JDITHER_NONE;
			} else if (keymatch(argv[argn], "ordered", 2)) {
				cinfo->dither_mode = JDITHER_ORDERED;
			} else
				usage();

		} else if (keymatch(arg, "debug", 1) || keymatch(arg, "verbose", 1)) {
			/* Enable debug printouts. */
			/* On first -d, print version identification */
			static boolean printed_version = FALSE;

			if (! printed_version) {
				fprintf(stderr, "Independent JPEG Group's DJPEG, version %s\n%s\n",
						JVERSION, JCOPYRIGHT);
				printed_version = TRUE;
			}
			cinfo->err->trace_level++;

		} else if (keymatch(arg, "fast", 1)) {
			/* Select recommended processing options for quick-and-dirty output. */
			cinfo->two_pass_quantize = FALSE;
			cinfo->dither_mode = JDITHER_ORDERED;
			if (! cinfo->quantize_colors) /* don't override an earlier -colors */
				cinfo->desired_number_of_colors = 216;
			cinfo->dct_method = JDCT_FASTEST;
			cinfo->do_fancy_upsampling = FALSE;

		} else if (keymatch(arg, "gif", 1)) {
			/* GIF output format. */
			requested_fmt = FMT_GIF;

		} else if (keymatch(arg, "grayscale", 2) || keymatch(arg, "greyscale",2)) {
			/* Force monochrome output. */
			cinfo->out_color_space = JCS_GRAYSCALE;

		} else if (keymatch(arg, "map", 3)) {
			/* Quantize to a color map taken from an input file. */
			if (++argn >= argc)	/* advance to next argument */
				usage();
			if (for_real) {		/* too expensive to do twice! */
#ifdef QUANT_2PASS_SUPPORTED	/* otherwise can't quantize to supplied map */
				FILE * mapfile;

				if ((mapfile = fopen(argv[argn], READ_BINARY)) == NULL) {
					fprintf(stderr, "%s: can't open %s\n", progname, argv[argn]);
					exit(EXIT_FAILURE);
				}
				read_color_map(cinfo, mapfile);
				fclose(mapfile);
				cinfo->quantize_colors = TRUE;
#else
				ERREXIT(cinfo, JERR_NOT_COMPILED);
#endif
			}

		} else if (keymatch(arg, "maxmemory", 3)) {
			/* Maximum memory in Kb (or Mb with 'm'). */
			long lval;
			char ch = 'x';

			if (++argn >= argc)	/* advance to next argument */
				usage();
			if (sscanf(argv[argn], "%ld%c", &lval, &ch) < 1)
				usage();
			if (ch == 'm' || ch == 'M')
				lval *= 1000L;
			cinfo->mem->max_memory_to_use = lval * 1000L;

		} else if (keymatch(arg, "nosmooth", 3)) {
			/* Suppress fancy upsampling */
			cinfo->do_fancy_upsampling = FALSE;

		} else if (keymatch(arg, "onepass", 3)) {
			/* Use fast one-pass quantization. */
			cinfo->two_pass_quantize = FALSE;

		} else if (keymatch(arg, "os2", 3)) {
			/* BMP output format (OS/2 flavor). */
			requested_fmt = FMT_OS2;

		} else if (keymatch(arg, "outfile", 4)) {
			/* Set output file name. */
			if (++argn >= argc)	/* advance to next argument */
				usage();
			outfilename = argv[argn];	/* save it away for later use */

		} else if (keymatch(arg, "pnm", 1) || keymatch(arg, "ppm", 1)) {
			/* PPM/PGM output format. */
			requested_fmt = FMT_PPM;

		} else if (keymatch(arg, "rle", 1)) {
			/* RLE output format. */
			requested_fmt = FMT_RLE;

		} else if (keymatch(arg, "scale", 1)) {
			/* Scale the output image by a fraction M/N. */
			if (++argn >= argc)	/* advance to next argument */
				usage();
			if (sscanf(argv[argn], "%d/%d",
					&cinfo->scale_num, &cinfo->scale_denom) != 2)
				usage();

		} else if (keymatch(arg, "targa", 1)) {
			/* Targa output format. */
			requested_fmt = FMT_TARGA;

		} else {
			usage();			/* bogus switch */
		}
	}

	return argn;			/* return index of next arg (file name) */
		}


/*
 * Marker processor for COM and interesting APPn markers.
 * This replaces the library's built-in processor, which just skips the marker.
 * We want to print out the marker as text, to the extent possible.
 * Note this code relies on a non-suspending data source.
 */

LOCAL(unsigned int)
jpeg_getc (j_decompress_ptr cinfo)
/* Read next byte */
{
	struct jpeg_source_mgr * datasrc = cinfo->src;

	if (datasrc->bytes_in_buffer == 0) {
		if (! (*datasrc->fill_input_buffer) (cinfo))
			ERREXIT(cinfo, JERR_CANT_SUSPEND);
	}
	datasrc->bytes_in_buffer--;
	return GETJOCTET(*datasrc->next_input_byte++);
}


METHODDEF(boolean)
print_text_marker (j_decompress_ptr cinfo)
{
	boolean traceit = (cinfo->err->trace_level >= 1);
	INT32 length;
	unsigned int ch;
	unsigned int lastch = 0;

	length = jpeg_getc(cinfo) << 8;
	length += jpeg_getc(cinfo);
	length -= 2;			/* discount the length word itself */

	if (traceit) {
		if (cinfo->unread_marker == JPEG_COM)
			fprintf(stderr, "Comment, length %ld:\n", (long) length);
		else			/* assume it is an APPn otherwise */
			fprintf(stderr, "APP%d, length %ld:\n",
					cinfo->unread_marker - JPEG_APP0, (long) length);
	}

	while (--length >= 0) {
		ch = jpeg_getc(cinfo);
		if (traceit) {
			/* Emit the character in a readable form.
			 * Nonprintables are converted to \nnn form,
			 * while \ is converted to \\.
			 * Newlines in CR, CR/LF, or LF form will be printed as one newline.
			 */
			if (ch == '\r') {
				fprintf(stderr, "\n");
			} else if (ch == '\n') {
				if (lastch != '\r')
					fprintf(stderr, "\n");
			} else if (ch == '\\') {
				fprintf(stderr, "\\\\");
			} else if (isprint(ch)) {
				putc(ch, stderr);
			} else {
				fprintf(stderr, "\\%03o", ch);
			}
			lastch = ch;
		}
	}

	if (traceit)
		fprintf(stderr, "\n");

	return TRUE;
}

#ifdef USE_NVM
char * extract_filename(char *str)
{
	int     ch = '/';
	size_t  len;
	char   *pdest;
	char   *inpfile = NULL;

	pdest = strrchr(str, ch);
	if(pdest == NULL )
	{
		printf( "Result:\t%c not found\n", ch );
		pdest = str;  // The whole name is a file in current path?
	}
	else
	{
		pdest++; // Skip the backslash itself.
	}
	// extract filename from file path
	len = strlen(pdest);
	inpfile = malloc(len+1);  // Make space for the zero.
	strncpy(inpfile, pdest, len+1);  // Copy including zero.
	return inpfile;
}
#endif


struct filestruct{
  char fname[256];
  unsigned int size;
};
struct filestruct *filelist=NULL;
static int g_idx=0;

/*
 * The main program.
 */

int
main (int argc, char **argv)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
#ifdef PROGRESS_REPORT
	struct cdjpeg_progress_mgr progress;
#endif
	int file_index;
	djpeg_dest_ptr dest_mgr = NULL;
	FILE * input_file;
	FILE * output_file;
	JDIMENSION num_scanlines;
	char *input_name = NULL;
	char *out_name= NULL;

#if 1
      nvinit(600);
#endif

#ifdef USE_NVML
		plp1 = pmemlog_create_jpeg();
#endif



	/* On Mac, fetch a command line. */
#ifdef USE_CCOMMAND
	argc = ccommand(&argv);
#endif

	input_name = (char *)malloc(256);
	assert(input_name);
	out_name = (char *)malloc(256);
	assert(out_name);


#ifdef USE_DIRECTORY

#if defined(USE_NVM) && defined(USE_NOMMAP)
	size_t sz=0;
	filelist = p_c_nvread_len((char *)"filelist",0,&sz);
	//fprintf (stderr, "Reading %lu, %zu\n",filelist, sz);
	int g_idx =0;
#else
	DIR *mydir = NULL;
	struct dirent *entry = NULL;

	mydir = opendir(argv[6]);
	if(!mydir) {
		fprintf(stderr,"failed %s\n",(char*)argv[6]);
		assert(mydir);
	}
	entry = readdir(mydir);
#endif


#if defined(USE_NVM) && defined(USE_NOMMAP)
    while(filelist[g_idx].size)
#else
	while(entry)
#endif
	{
		char *input = NULL;
		char *entryname = NULL;
#if defined(USE_NVM) && defined(USE_NOMMAP)
		size_t len = strlen(filelist[g_idx].fname);
		//fprintf (stdout, "%s, %u \n",filelist[g_idx].fname, filelist[g_idx].size);
		//fprintf (stdout, "%s, %u \n",filelist[g_idx].fname, g_idx);
		entryname = filelist[g_idx].fname;
		g_idx++;
#else
		size_t len = strlen(entry->d_name);
		entryname = entry->d_name;
#endif
		if(len < 4)
			goto next;
#endif
		progname = argv[0];
		if (progname == NULL || progname[0] == 0)
			progname = "djpeg";		/* in case C library doesn't provide it */

		/* Initialize the JPEG decompression object with default error handling. */
		cinfo.err = jpeg_std_error(&jerr);
		jpeg_create_decompress(&cinfo);
		/* Add some application-specific error messages (from cderror.h) */
		jerr.addon_message_table = cdjpeg_message_table;
		jerr.first_addon_message = JMSG_FIRSTADDONCODE;
		jerr.last_addon_message = JMSG_LASTADDONCODE;

		/* Insert custom marker processor for COM and APP12.
		 * APP12 is used by some digital camera makers for textual info,
		 * so we provide the ability to display it as text.
		 * If you like, additional APPn marker types can be selected for display,
		 * but don't try to override APP0 or APP14 this way (see libjpeg.doc).
		 */
		jpeg_set_marker_processor(&cinfo, JPEG_COM, print_text_marker);
		jpeg_set_marker_processor(&cinfo, JPEG_APP0+12, print_text_marker);

		/* Now safe to enable signal catcher. */
#ifdef NEED_SIGNAL_CATCHER
		enable_signal_catcher((j_common_ptr) &cinfo);
#endif

		/* Scan command line to find file names. */
		/* It is convenient to use just one switch-parsing routine, but the switch
		 * values read here are ignored; we will rescan the switches after opening
		 * the input file.
		 * (Exception: tracing level set here controls verbosity for COM markers
		 * found during jpeg_read_header...)
		 */

		file_index = parse_switches(&cinfo, argc, argv, 0, FALSE);

#ifdef TWO_FILE_COMMANDLINE
		/* Must have either -outfile switch or explicit output file name */
		if (outfilename == NULL) {
			if (file_index != argc-2) {
				fprintf(stderr, "%s: must name one input and one output file\n",
						progname);
				usage();
			}
			outfilename = argv[file_index+1];
		} else {
			if (file_index != argc-1) {
				fprintf(stderr, "%s: must name one input and one output file\n",
						progname);
				usage();
			}
		}
#else
		/* Unix style: expect zero or one file name */
		if (file_index < argc-1) {
			fprintf(stderr, "%s: only one input file\n", progname);
			usage();
		}
#endif /* TWO_FILE_COMMANDLINE */


#ifdef USE_DIRECTORY

		if(!strstr(entryname, ".jpg") || strstr(entryname, ".bmp")){
			goto next;
		}
		strcpy(input_name,argv[file_index]);
		strcat(input_name,entryname);

		strcpy(out_name,(char*)"out/");
		strcat(out_name,entryname);
		strcat(out_name,".bmp");
#else
		strcpy(input_name,argv[file_index]);
		strcpy(out_name,outfilename);
#endif
		//fprintf(stdout,"input name %s \n",input_name);

#if !defined(USE_NVM) || !defined(USE_NOMMAP)

		/* Open the input file. */
		if (file_index < argc) {
			if ((input_file = fopen(input_name, READ_BINARY)) == NULL) {
				fprintf(stderr, "%s: can't open %s\n", progname, argv[file_index]);
				exit(EXIT_FAILURE);
			}
		} else {
			/* default input file is stdin */
			input_file = read_stdin();
		}


		/* Open the output file. */
		if (outfilename != NULL) {
			if ((output_file = fopen(out_name, WRITE_BINARY)) == NULL) {
				fprintf(stderr, "%s: can't open %s\n", progname, outfilename);
				exit(EXIT_FAILURE);
			}
		} else {
			/* default output file is stdout */
			output_file = write_stdout();
		}
#endif

#ifdef PROGRESS_REPORT
		start_progress_monitor((j_common_ptr) &cinfo, &progress);
#endif
		/* Specify data source for decompression */
		jpeg_stdio_src(&cinfo, input_file);
#ifdef USE_NVM
		void *nvptr = NULL;
		size_t fsize = 0;
		char *fname = extract_filename(input_name);
		char *outfname = extract_filename(out_name);
		nvptr = p_c_nvread_len(fname,0,&fsize);
		assert(nvptr);
		cinfo.src->nvbuffer = nvptr;
		cinfo.src->rdbytes = 0;
		cinfo.src->tot_bytes = fsize;
		//fprintf(stderr,"file size %zu %s\n", fsize, fname);

#endif
		/* Read file header, set default decompression parameters */
		(void) jpeg_read_header(&cinfo, TRUE);

		/* Adjust default decompression parameters by re-parsing the options */
		file_index = parse_switches(&cinfo, argc, argv, 0, TRUE);

		/* Initialize the output module now to let it override any crucial
		 * option settings (for instance, GIF wants to force color quantization).
		 */
		switch (requested_fmt) {
#ifdef BMP_SUPPORTED
		case FMT_BMP:
#ifdef USE_NVM
			dest_mgr = jinit_write_bmp_nv(&cinfo, FALSE,outfname);
			dest_mgr->nvoutbuffer = cinfo.nvoutbuffer;
			//dest_mgr->nvoutsize = cinfo.nvoutsize;
			//fprintf(stderr,"cinfo->nvoutsize %u\n", cinfo.nvoutsize);
#else
			dest_mgr = jinit_write_bmp(&cinfo, FALSE);
#endif
			break;
		case FMT_OS2:
#ifdef USE_NVM
			dest_mgr = jinit_write_bmp_nv(&cinfo, FALSE,outfname);
#else
			dest_mgr = jinit_write_bmp(&cinfo, TRUE);
#endif
			break;
#endif
#ifdef GIF_SUPPORTED
		case FMT_GIF:
			dest_mgr = jinit_write_gif(&cinfo);
			break;
#endif
#ifdef PPM_SUPPORTED
		case FMT_PPM:
			dest_mgr = jinit_write_ppm(&cinfo);
			break;
#endif
#ifdef RLE_SUPPORTED
		case FMT_RLE:
			dest_mgr = jinit_write_rle(&cinfo);
			break;
#endif
#ifdef TARGA_SUPPORTED
		case FMT_TARGA:
			dest_mgr = jinit_write_targa(&cinfo);
			break;
#endif
		default:
			ERREXIT(&cinfo, JERR_UNSUPPORTED_FORMAT);
			break;
		}
		dest_mgr->output_file = output_file;

		/* Start decompressor */
		(void) jpeg_start_decompress(&cinfo);

		/* Write output file header */
		(*dest_mgr->start_output) (&cinfo, dest_mgr);

		/* Process data */
		while (cinfo.output_scanline < cinfo.output_height) {
			num_scanlines = jpeg_read_scanlines(&cinfo, dest_mgr->buffer,
					dest_mgr->buffer_height);
			(*dest_mgr->put_pixel_rows) (&cinfo, dest_mgr, num_scanlines);
		}

#ifdef PROGRESS_REPORT
		/* Hack: count final pass as done in case finish_output does an extra pass.
		 * The library won't have updated completed_passes.
		 */
		progress.pub.completed_passes = progress.pub.total_passes;
#endif

		/* Finish decompression and release memory.
		 * I must do it in this order because output module has allocated memory
		 * of lifespan JPOOL_IMAGE; it needs to finish before releasing memory.
		 */
		(*dest_mgr->finish_output) (&cinfo, dest_mgr);
		(void) jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);

#if !defined(USE_NVM) || !defined(USE_NOMMAP)
		/* Close files, if we opened them */
		if (input_file != stdin)
			fclose(input_file);

		if (output_file != stdout)
			fclose(output_file);
	
		 //assert(cinfo.src->nvbuffer);
		 //assert(dest_mgr->nvoutbuffer);
#endif

#ifdef USE_NVM
		 //p_c_mmap_free(fname, cinfo.src->nvbuffer);

		 dest_mgr->nvoutsize = cinfo.nvoutsize;

#ifdef USE_NVML
		 if(dest_mgr->nvoutsize && dest_mgr->nvoutbuffer)
			 pmemlog_append_jpeg(dest_mgr->nvoutbuffer, dest_mgr->nvoutsize);
#endif

		 //fprintf(stderr,"outfname %s, dest_mgr->nvoutsize %u \n",
	         //					outfname, cinfo.nvoutsize);
		 //p_c_nvcommitsz(outfname, cinfo.nvoutsize);
		
		 //p_c_mmap_free(outfname, dest_mgr->nvoutbuffer);
#endif


#ifdef PROGRESS_REPORT
		end_progress_monitor((j_common_ptr) &cinfo);
#endif

#ifdef USE_DIRECTORY
		next:
#if !defined(USE_NVM) || !defined(USE_NOMMAP)
		entry = readdir(mydir);
#endif
		if(out_name)
			memset(out_name,0,256);
		if(input_name)
		memset(input_name,0,256);
	}
#endif

#ifdef USE_NVML
	   pmemlog_rewind(plp1);
	   pmemobj_close((PMEMobjpool *)plp1);
#endif


	//c_app_stop_(1);

	/* All done. */
	exit(jerr.num_warnings ? EXIT_WARNING : EXIT_SUCCESS);
	return 0;			/* suppress no-return-value warnings */
}
