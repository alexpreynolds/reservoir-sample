#include "reservoir-sample.h"

int main(int argc, char** argv) 
{
    long k;
    offset_reservoir *offset_reservoir_ptr = NULL;
    char *in_filename = NULL;
    file_mmap *in_file_mmap_ptr = NULL;
    boolean preserve_output_order;
    boolean mmap_in_file;
    boolean cstdio_in_file;
    boolean hybrid_in_file;
    boolean sample_without_replacement;
    boolean sample_with_replacement;
    boolean sample_size_specified;

    parse_command_line_options(argc, argv);
    k = reservoir_sample_client_global_args.k;
    in_filename = reservoir_sample_client_global_args.filenames[0];
    mmap_in_file = reservoir_sample_client_global_args.mmap;
    cstdio_in_file = reservoir_sample_client_global_args.cstdio;
    hybrid_in_file = reservoir_sample_client_global_args.hybrid;
    preserve_output_order = reservoir_sample_client_global_args.preserve_order;
    sample_without_replacement = reservoir_sample_client_global_args.sample_without_replacement;
    sample_with_replacement = reservoir_sample_client_global_args.sample_with_replacement;
    sample_size_specified = reservoir_sample_client_global_args.sample_size_specified;

    if ((k <= 0) || (!in_filename)) {
        print_usage(stderr);
        return EXIT_FAILURE;
    }

    /* seed random number generator */
    srand(time(NULL));

    offset_reservoir_ptr = new_offset_reservoir_ptr(k);

    if (sample_without_replacement) {
        if ((hybrid_in_file) || (cstdio_in_file)) {
            if (sample_size_specified)
                offset_reservoir_sample_input_via_cstdio_with_fixed_k(in_filename, &offset_reservoir_ptr);
            else
                offset_reservoir_shuffle_input_via_cstdio_with_unspecified_k(in_filename, &offset_reservoir_ptr);
        }
        else if (mmap_in_file) {
            in_file_mmap_ptr = new_file_mmap(in_filename);
            if (sample_size_specified)
                offset_reservoir_sample_input_via_mmap_with_fixed_k(in_file_mmap_ptr, &offset_reservoir_ptr);
            else {
                /* offset_reservoir_sample_input_via_mmap_with_unspecified_k(in_filename, &offset_reservoir_ptr); */
                fprintf(stderr, "Error: This application does not yet support sampling without replacement without specified k\n");
                return EXIT_FAILURE;
            }
        }
    }
    else if (sample_with_replacement) {
        fprintf(stderr, "Error: This application does not yet support sampling with replacement\n");
        return EXIT_FAILURE;
    }

#ifdef DEBUG
    print_offset_reservoir_ptr(offset_reservoir_ptr);
#endif

    if (preserve_output_order) {
        sort_offset_reservoir_ptr_offsets(&offset_reservoir_ptr);
#ifdef DEBUG
        print_offset_reservoir_ptr(offset_reservoir_ptr);
#endif
    }

    if (hybrid_in_file) {
        in_file_mmap_ptr = new_file_mmap(in_filename);
        print_offset_reservoir_sample_via_mmap(in_file_mmap_ptr, offset_reservoir_ptr);
        free_file_mmap(&in_file_mmap_ptr);
    }
    else if (cstdio_in_file) {
        if (preserve_output_order)
            print_sorted_offset_reservoir_sample_via_cstdio(in_filename, offset_reservoir_ptr);
        else
            print_unsorted_offset_reservoir_sample_via_cstdio(in_filename, offset_reservoir_ptr);
    }
    else if (mmap_in_file) {
        print_offset_reservoir_sample_via_mmap(in_file_mmap_ptr, offset_reservoir_ptr);
        free_file_mmap(&in_file_mmap_ptr);
    }

    free_offset_reservoir_ptr(&offset_reservoir_ptr);

    return EXIT_SUCCESS;
}

offset_reservoir * new_offset_reservoir_ptr(const long len)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: new_offset_reservoir_ptr()\n");
#endif

    offset_reservoir *res = NULL;
    off_t *offsets = NULL;
    
    offsets = malloc(sizeof(off_t) * len);
    if (!offsets) {
        fprintf(stderr, "Debug: offsets instance is NULL\n");
        exit(EXIT_FAILURE);
    }

    res = malloc(sizeof(offset_reservoir));
    if (!res) {
        fprintf(stderr, "Debug: offset_reservoir instance res is NULL\n");
        exit(EXIT_FAILURE);
    }

    res->num_offsets = len;
    res->offsets = offsets;

    return res;
}

void free_offset_reservoir_ptr(offset_reservoir **res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: free_offset_reservoir_ptr()\n");
#endif

    if (!*res_ptr) {
        fprintf(stderr, "Error: Offset reservoir pointer instance is NULL\n");
        exit(EXIT_FAILURE);
    }

    if ((*res_ptr)->offsets) {
        free((*res_ptr)->offsets);
        (*res_ptr)->offsets = NULL;
    }

    free(*res_ptr);
    *res_ptr = NULL;
}

void print_offset_reservoir_ptr(const offset_reservoir *res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: print_offset_reservoir_ptr()\n");
#endif

    int idx;

    for (idx = 0; idx < res_ptr->num_offsets; ++idx)
        fprintf(stdout, "[%012d] %012lld\n", idx, (long long int) res_ptr->offsets[idx]);
}

void offset_reservoir_sample_input_via_cstdio_with_fixed_k(const char *in_fn, offset_reservoir **res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: offset_reservoir_sample_input_offsets_with_fixed_k()\n");
#endif

    int not_stdin = 0;
    FILE *in_file_ptr = NULL;
    char in_line[LINE_LENGTH_VALUE + 1];
    off_t start_offset = 0;
    off_t stop_offset = 0;
    long k = (*res_ptr)->num_offsets;
    double p_replacement = 0.0;
    long rand_idx = 0;
    long ln_idx = 0;

    not_stdin = strcmp(in_fn, "-");
    in_file_ptr = (not_stdin) ? fopen(in_fn, "r") : stdin;

    if (in_file_ptr == stdin) {
        fprintf(stderr, "Error: Stdin not yet supported with this function\n");
        exit(EXIT_FAILURE);
    }
    
    in_line[LINE_LENGTH_VALUE] = '1';

    /* read offsets into reservoir, replacing random offsets when the line counter is greater than k */
    while (fgets(in_line, LINE_LENGTH_VALUE + 1, in_file_ptr)) 
        {
            if (ln_idx < k) {
#ifdef DEBUG
                fprintf(stderr, "Debug: Adding node at idx %012d with offset %012lld\n", ln_idx, (long long int) start_offset);
#endif
                (*res_ptr)->offsets[ln_idx] = start_offset;
            }
            else {
                p_replacement = (double) k / (ln_idx + 1);
                rand_idx = rand() % k;
                if (p_replacement > ((double) rand() / RAND_MAX)) {
#ifdef DEBUG
                    fprintf(stderr, "Debug: Replacing random offset %012d for line %012d with probability %f\n", rand_idx, ln_idx, p_replacement);
#endif
                    (*res_ptr)->offsets[rand_idx] = start_offset;
                }
            }
            stop_offset = ftell(in_file_ptr);
#ifdef DEBUG
            fprintf(stderr, "Debug: [%012lld - %012lld]\n", (long long int) start_offset, (long long int) stop_offset);
#endif
            start_offset = stop_offset;
            ln_idx++;
        }

    /* for when there are fewer lines than the sample size */
    if (ln_idx < k)
        (*res_ptr)->num_offsets = ln_idx;

    fclose(in_file_ptr);
}

void offset_reservoir_shuffle_input_via_cstdio_with_unspecified_k(const char *in_fn, offset_reservoir **res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: offset_reservoir_shuffle_input_via_cstdio_with_unspecified_k()\n");
#endif
    
    int not_stdin = 0;
    FILE *in_file_ptr = NULL;
    char in_line[LINE_LENGTH_VALUE + 1];
    off_t start_offset = 0;
    off_t stop_offset = 0;
    long k = (*res_ptr)->num_offsets;
    long shuf_idx = 0;
    long rand_idx = 0;
    long ln_idx = 0;
    off_t *resized_offsets = NULL;
    off_t temp_offset = 0;

    not_stdin = strcmp(in_fn, "-");
    in_file_ptr = (not_stdin) ? fopen(in_fn, "r") : stdin;

    if (in_file_ptr == stdin) {
        fprintf(stderr, "Error: Stdin not yet supported with this function\n");
        exit(EXIT_FAILURE);
    }
    
    in_line[LINE_LENGTH_VALUE] = '1';

    /* read all offsets into reservoir, reallocating memory as needed */
    while (fgets(in_line, LINE_LENGTH_VALUE + 1, in_file_ptr)) 
        {
            if (ln_idx == k) {
                k += DEFAULT_SAMPLE_SIZE_INCREMENT;
                resized_offsets = realloc((*res_ptr)->offsets, sizeof(off_t) * k);
                if (!resized_offsets) {
                    fprintf(stderr, "Error: Could not allocate memory for resized offset array\n");
                    exit(EXIT_FAILURE);
                }
                (*res_ptr)->offsets = resized_offsets;
            }
            (*res_ptr)->offsets[ln_idx] = start_offset;
            stop_offset = ftell(in_file_ptr);
            start_offset = stop_offset;
            ln_idx++;
        }
    (*res_ptr)->num_offsets = ln_idx;

    /* shuffle offsets via Fisher-Yates */
    for (shuf_idx = (ln_idx - 1); shuf_idx > 0; --shuf_idx) {
        rand_idx = ((double) rand() / RAND_MAX) * shuf_idx;
        temp_offset = (*res_ptr)->offsets[shuf_idx];
        (*res_ptr)->offsets[shuf_idx] = (*res_ptr)->offsets[rand_idx];
        (*res_ptr)->offsets[rand_idx] = temp_offset;
    }

    fclose(in_file_ptr);    
}

void offset_reservoir_sample_input_via_mmap_with_fixed_k(file_mmap *in_mmap, offset_reservoir **res_ptr) 
{
#ifdef DEBUG
    fprintf(stderr, "Debug: offset_reservoir_sample_input_via_mmap_with_fixed_k()\n");
#endif

    size_t offset_idx;
    off_t start_offset = 0;
    off_t stop_offset = 0;
    int k = (*res_ptr)->num_offsets;
    int ln_idx = 0;
    double p_replacement = 0.0;
    int rand_idx = 0;

    for (offset_idx = 0; offset_idx < in_mmap->size; ++offset_idx) {
        if (in_mmap->map[offset_idx] == '\n') {
            if (ln_idx < k) {
#ifdef DEBUG
                fprintf(stderr, "Debug: Adding offset at idx %012d with offset value %012lld\n", ln_idx, (long long int) start_offset);
#endif
                (*res_ptr)->offsets[ln_idx] = start_offset;
            }
            else {
                p_replacement = (double) k / (ln_idx + 1);
                rand_idx = rand() % k;
                if (p_replacement > ((double) rand() / RAND_MAX))
                    (*res_ptr)->offsets[rand_idx] = start_offset;
            }
            stop_offset = offset_idx;
            start_offset = stop_offset + 1;
            ln_idx++;
        }
    }

    if (ln_idx < k)
        (*res_ptr)->num_offsets = ln_idx;
}

void offset_reservoir_sample_input_via_mmap_with_unspecified_k(file_mmap *in_mmap, offset_reservoir **res_ptr) 
{
#ifdef DEBUG
    fprintf(stderr, "Debug: offset_reservoir_sample_input_via_mmap_with_unspecified_k()\n");
#endif
}

void sort_offset_reservoir_ptr_offsets(offset_reservoir **res_ptr) 
{
#ifdef DEBUG
    fprintf(stderr, "Debug: sort_offset_reservoir_ptr_offsets()\n");
#endif
    qsort( (*res_ptr)->offsets, (*res_ptr)->num_offsets, sizeof(off_t), offset_compare );
}

int offset_compare(const void *off1, const void *off2)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: offset_compare()\n");
    fprintf(stderr, "Debug: Comparing: %012lld and %012lld\n", *(off_t *)off1, *(off_t *)off2);
#endif
    int off_diff = (*(off_t *)off1) - (*(off_t *)off2);
    return (off_diff > 0) ? 1 : -1;
} 

void print_offset_reservoir_sample_via_mmap(const file_mmap *in_mmap, offset_reservoir *res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: print_offset_reservoir_sample_via_mmap()\n");
#endif

    int res_idx;
    off_t mmap_idx;
    off_t current_offset;
    
    for (res_idx = 0; res_idx < res_ptr->num_offsets; ++res_idx) {
        current_offset = res_ptr->offsets[res_idx];
        for (mmap_idx = current_offset; mmap_idx < current_offset + LINE_LENGTH_VALUE; ++mmap_idx) {
            fprintf(stdout, "%c", in_mmap->map[mmap_idx]);
            if (in_mmap->map[mmap_idx] == '\n')
                break;
        }
    }
}

void print_sorted_offset_reservoir_sample_via_cstdio(const char *in_fn, offset_reservoir *res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: print_sorted_offset_reservoir_sample_via_cstdio()\n");
#endif

    int not_stdin = 0;
    FILE *in_file_ptr = NULL;
    int idx;
    char in_line[LINE_LENGTH_VALUE + 1];
    off_t previous_offset = 0;
    size_t previous_line_length = 0;

    not_stdin = strcmp(in_fn, "-");
    in_file_ptr = (not_stdin) ? fopen(in_fn, "r") : stdin;
    
    if (in_file_ptr == stdin) {
        fprintf(stderr, "Error: Stdin not yet supported with this function\n");
        exit(EXIT_FAILURE);
    }

    for (idx = 0; idx < res_ptr->num_offsets; ++idx) {
        /* 
           we use SEEK_CUR to jump from wherever the file pointer is now, instead of 
           from the start of the file -- we can do this because the offsets are in
           sorted order
        */
        fseek(in_file_ptr, res_ptr->offsets[idx] - previous_offset - previous_line_length, SEEK_CUR);
        fgets(in_line, LINE_LENGTH_VALUE + 1, in_file_ptr);
        fprintf(stdout, "%s", in_line);
        previous_line_length = strlen(in_line);
        previous_offset = res_ptr->offsets[idx];
    }

    fclose(in_file_ptr);
}

void print_unsorted_offset_reservoir_sample_via_cstdio(const char *in_fn, offset_reservoir *res_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: print_unsorted_offset_reservoir_sample_via_cstdio()\n");
#endif

    int not_stdin = 0;
    FILE *in_file_ptr = NULL;
    int idx;
    char in_line[LINE_LENGTH_VALUE + 1];

    not_stdin = strcmp(in_fn, "-");
    in_file_ptr = (not_stdin) ? fopen(in_fn, "r") : stdin;
    
    if (in_file_ptr == stdin) {
        fprintf(stderr, "Error: Stdin not yet supported with this function\n");
        exit(EXIT_FAILURE);
    }

    for (idx = 0; idx < res_ptr->num_offsets; ++idx) {
        /* 
           we use SEEK_SET to jump from the start of the file, as the offsets are unsorted
        */
        fseek(in_file_ptr, res_ptr->offsets[idx], SEEK_SET);
        fgets(in_line, LINE_LENGTH_VALUE + 1, in_file_ptr);
        fprintf(stdout, "%s", in_line);
    }

    fclose(in_file_ptr);
}

file_mmap * new_file_mmap(const char *in_fn)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: new_file_mmap()\n");
#endif
    
    file_mmap *mmap_ptr = NULL;
    boolean not_stdin = kTrue;

    mmap_ptr = malloc(sizeof(file_mmap));
    if (!mmap_ptr) {
        fprintf(stderr, "Error: Mmap pointer is NULL\n");
        exit(EXIT_FAILURE);
    }

    not_stdin = (strcmp(in_fn, "-")) ? kTrue : kFalse;
    
    if (!not_stdin) {
        fprintf(stderr, "Error: Stdin not yet supported with mmap setup function\n");
        exit(EXIT_FAILURE);
    }

    mmap_ptr->fn = NULL;
    mmap_ptr->fn = malloc(strlen(in_fn) + 1);
    if (!mmap_ptr->fn) {
        fprintf(stderr, "Error: Mmap pointer's filename pointer is NULL\n");
        exit(EXIT_FAILURE);
    }
    strncpy(mmap_ptr->fn, in_fn, strlen(in_fn) + 1);
    mmap_ptr->fd = open(mmap_ptr->fn, O_RDONLY);
    mmap_ptr->status = fstat(mmap_ptr->fd, &(mmap_ptr->s));
    mmap_ptr->size = mmap_ptr->s.st_size;
    mmap_ptr->map = (char *) mmap(NULL, 
                                  mmap_ptr->size, 
                                  PROT_READ, 
                                  MAP_SHARED,
                                  mmap_ptr->fd, 
                                  0);
    if (mmap_ptr->map == MAP_FAILED) {
        fprintf(stderr, "Error: Mmap pointer map failed\n");
        exit(EXIT_FAILURE);
    }

    return mmap_ptr;
}

void free_file_mmap(file_mmap **mmap_ptr)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: free_file_mmap()\n");
#endif

    close((*mmap_ptr)->fd);
    munmap((*mmap_ptr)->map, (*mmap_ptr)->size);
    free((*mmap_ptr)->fn);
    (*mmap_ptr)->fn = NULL;
    free(*mmap_ptr);
    *mmap_ptr = NULL;
}

void initialize_globals()
{
    reservoir_sample_client_global_args.sample_size_specified = kFalse;
    reservoir_sample_client_global_args.sample_without_replacement = kTrue;
    reservoir_sample_client_global_args.sample_with_replacement = kFalse;
    reservoir_sample_client_global_args.shuffle = kTrue;
    reservoir_sample_client_global_args.preserve_order = kFalse;
    reservoir_sample_client_global_args.hybrid = kFalse;
    reservoir_sample_client_global_args.mmap = kTrue;
    reservoir_sample_client_global_args.cstdio = kFalse;
    reservoir_sample_client_global_args.k = 0;
    reservoir_sample_client_global_args.filenames = NULL;
    reservoir_sample_client_global_args.num_filenames = 0;
}

void parse_command_line_options(int argc, char **argv)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: parse_command_line_options()\n");
#endif

    int client_long_index;
    int client_opt = getopt_long(argc, 
                                 argv, 
                                 reservoir_sample_client_opt_string, 
                                 reservoir_sample_client_long_options, 
                                 &client_long_index);

    int order_type_flags = 0;
    int sample_type_flags = 0;
    int io_type_flags = 0;
    int sample_size_flag = kFalse;

    opterr = 0; /* disable error reporting by GNU getopt */
    initialize_globals();
    
    while (client_opt != -1) 
        {
            switch (client_opt) 
                {
                case 'k':
                    reservoir_sample_client_global_args.k = atoi(optarg);
                    sample_size_flag = kTrue;
                    break;
                case 'o':
                    reservoir_sample_client_global_args.sample_without_replacement = kTrue;
                    sample_type_flags++;
                    break;
                case 'r':
                    reservoir_sample_client_global_args.sample_with_replacement = kTrue;
                    reservoir_sample_client_global_args.sample_without_replacement = kFalse;
                    sample_type_flags++;
                    break;
                case 's':
                    reservoir_sample_client_global_args.shuffle = kTrue;
                    reservoir_sample_client_global_args.preserve_order = kFalse;
                    order_type_flags++;
                    break;
                case 'p':
                    reservoir_sample_client_global_args.preserve_order = kTrue;
                    reservoir_sample_client_global_args.shuffle = kFalse;
                    order_type_flags++;
                    break;
                case 'y':
                    reservoir_sample_client_global_args.hybrid = kTrue;
                    reservoir_sample_client_global_args.mmap = kFalse;
                    io_type_flags++;
                    break;
                case 'm':
                    reservoir_sample_client_global_args.mmap = kTrue;
                    io_type_flags++;
                    break;
                case 'c':
                    reservoir_sample_client_global_args.cstdio = kTrue;
                    reservoir_sample_client_global_args.mmap = kFalse;
                    io_type_flags++;
                    break;
                case 'h':
                    print_usage(stdout);
                    exit(EXIT_SUCCESS);
                case '?':
                    print_usage(stderr);
                    exit(EXIT_FAILURE);
                default:
                    break;
                }
            client_opt = getopt_long(argc, 
                                     argv, 
                                     reservoir_sample_client_opt_string, 
                                     reservoir_sample_client_long_options, 
                                     &client_long_index);
        }

    reservoir_sample_client_global_args.filenames = argv + optind;
    reservoir_sample_client_global_args.num_filenames = argc - optind;

    /* check input */

    if (!sample_size_flag)
        reservoir_sample_client_global_args.k += DEFAULT_SAMPLE_SIZE_INCREMENT;
    else
        reservoir_sample_client_global_args.sample_size_specified = kTrue;

    if ((order_type_flags > 1) ||
        (sample_type_flags > 1) ||
        (io_type_flags > 1) ||
        (reservoir_sample_client_global_args.k < 0) ||
        (reservoir_sample_client_global_args.num_filenames != 1))
        {
            print_usage(stderr);
            exit(EXIT_FAILURE);
        }
}

void print_usage(FILE *stream)
{
#ifdef DEBUG
    fprintf(stderr, "Debug: print_usage()\n");
#endif

    fprintf(stream, 
            "%s\n" \
            "  version: %s\n" \
            "  author:  %s\n" \
            "%s\n", 
            name, 
            version,
            authors,
            usage);
}
