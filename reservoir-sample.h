#ifndef RESERVOIR_SAMPLE_H
#define RESERVOIR_SAMPLE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <getopt.h>

#define RS_VERSION "1.0"
#define DEFAULT_OFFSET_VALUE -1
#define DEFAULT_SAMPLE_SIZE_INCREMENT 10000
#define LINE_LENGTH_VALUE 65536

typedef int boolean;
extern const boolean kTrue;
extern const boolean kFalse;

const boolean kTrue = 1;
const boolean kFalse = 0;

typedef struct offset_reservoir offset_reservoir;
typedef struct file_mmap file_mmap;

struct offset_reservoir {
    long num_offsets;
    off_t *offsets;
};

struct file_mmap {
    int fd;
    char *fn;
    struct stat s;
    int status;
    size_t size;
    char *map;
};

static const char *name = "reservoir-sample";
static const char *version = RS_VERSION;
static const char *authors = "Alex Reynolds";
static const char *usage = "\n" \
    "Usage: reservoir-sample [--sample-size=n] [--sample-without-replacement | --sample-with-replacement] [--shuffle | --preserve-order] [--hybrid | --mmap | --cstdio] <newline-delimited-file>\n" \
    "\n" \
    "  Performs reservoir sampling (http://dx.doi.org/10.1145/3147.3165) on very large input\n" \
    "  files that are delimited by newline characters. The approach used in this application\n" \
    "  reduces memory usage by storing a pool of byte offsets to the start of each line, instead\n" \
    "  of the line elements themselves.\n\n" \
    "  Process Flags:\n\n" \
    "  --sample-size=n               | -k n    Number of samples to retrieve (n = positive integer; optional)\n" \
    "  --sample-without-replacement  | -o      Sample without replacement (default)\n" \
    "  --sample-with-replacement     | -r      Sample with replacement (optional)\n" \
    "  --shuffle                     | -s      Shuffle sample before printing to standard output (default)\n" \
    "  --preserve-order              | -p      Preserve sample order before printing to standard output (optional)\n" \
    "  --mmap                        | -m      Use memory mapping for handling input file (default)\n" \
    "  --cstdio                      | -c      Use C I/O routines for handling input file (optional)\n" \
    "  --hybrid                      | -y      Use hybrid of C I/O routines and memory mapping for handling input file (optional)\n" \
    "  --help                        | -h      Show this usage message\n";

static struct reservoir_sample_client_global_args_t {
    boolean shuffle;
    boolean preserve_order;
    boolean hybrid;
    boolean mmap;
    boolean cstdio;
    boolean sample_without_replacement;
    boolean sample_with_replacement;
    boolean sample_size_specified;
    long k;
    char **filenames;
    int num_filenames;
} reservoir_sample_client_global_args;

static struct option reservoir_sample_client_long_options[] = {
    { "sample-size",			optional_argument,	NULL,	'k' },
    { "sample-without-replacement",	no_argument,		NULL,	'o' },
    { "sample-with-replacement",	no_argument,		NULL,	'r' },
    { "shuffle",			no_argument,		NULL,	's' },
    { "preserve-order",			no_argument,		NULL,	'p' },
    { "hybrid",	        		no_argument,		NULL,	'y' },
    { "mmap",	        		no_argument,		NULL,	'm' },
    { "cstdio",	        		no_argument,		NULL,	'c' },
    { "help",				no_argument,		NULL,	'h' },
    { NULL,				no_argument,		NULL,	 0  }
}; 

static const char *reservoir_sample_client_opt_string = "k:orspymch?";

offset_reservoir * new_offset_reservoir_ptr(const long len);
void free_offset_reservoir_ptr(offset_reservoir **res_ptr);
void print_offset_reservoir_ptr(const offset_reservoir *res_ptr);
void offset_reservoir_sample_input_via_cstdio_with_fixed_k(const char *in_fn, offset_reservoir **res_ptr);
void offset_reservoir_shuffle_input_via_cstdio_with_unspecified_k(const char *in_fn, offset_reservoir **res_ptr);
void offset_reservoir_sample_input_via_mmap_with_fixed_k(file_mmap *in_mmap, offset_reservoir **res_ptr);
void offset_reservoir_sample_input_via_mmap_with_unspecified_k(file_mmap *in_mmap, offset_reservoir **res_ptr);
void sort_offset_reservoir_ptr_offsets(offset_reservoir **res_ptr);
int offset_compare(const void *off1, const void *off2);
void print_offset_reservoir_sample_via_mmap(const file_mmap *in_mmap, offset_reservoir *res_ptr);
void print_sorted_offset_reservoir_sample_via_cstdio(const char *in_fn, offset_reservoir *res_ptr);
void print_unsorted_offset_reservoir_sample_via_cstdio(const char *in_fn, offset_reservoir *res_ptr);
file_mmap * new_file_mmap(const char *in_fn);
void free_file_mmap(file_mmap **mmap_ptr);
void initialize_globals();
void parse_command_line_options(int argc, char **argv);
void print_usage(FILE *stream);

#endif
