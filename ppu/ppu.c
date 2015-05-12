#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <libspe2.h>
#include <pthread.h>

#include "../cmp.h"

extern spe_program_handle_t spu;

#define MAX_SPU_THREADS   8

void *ppu_pthread_function(void *thread_arg) {

    spe_context_ptr_t ctx;

    /* Create SPE context */
    if ((ctx = spe_context_create(0, NULL)) == NULL) {
        perror("Failed creating context");
        exit(1);
    }

    /* Load SPE program into context */
    if (spe_program_load (ctx, &spu)) {
        perror("Failed loading program");
        exit(1);
    }

    /* Run SPE context for compression */
    ((struct args *) thread_arg)->mode_op = MODE_COMP;
    unsigned int entry = SPE_DEFAULT_ENTRY;
    if (spe_context_run(ctx, &entry, 0, thread_arg, NULL, NULL) < 0) {  
        perror("Failed running context");
        exit(1);
    }

    /* Run SPE context for decompression */
    ((struct args *) thread_arg)->mode_op = MODE_DECOMP;
    entry = SPE_DEFAULT_ENTRY;
    if (spe_context_run(ctx, &entry, 0, thread_arg, NULL, NULL) < 0) {  
        perror("Failed running context");
        exit(1);
    }

    /* Destroy context */
    if (spe_context_destroy(ctx) != 0) {
        perror("Failed destroying context");
        exit(1);
    }

    pthread_exit(NULL);
}

void compress_parallel(struct img *image, struct c_img *c_image, struct img *d_image, mode_vect_t mode_vect, mode_dma_t mode_dma, int spu_num) {

    pthread_t threads[MAX_SPU_THREADS];
    struct args thread_arg[MAX_SPU_THREADS] __attribute__ ((aligned(16)));

    /* set image attributes */
    c_image->width = image->width;
    c_image->height = image->height;
    c_image->blocks = _alloc((image->width / BLOCK_SIZE) * (image->height / BLOCK_SIZE) * sizeof(struct block));

    d_image->width = image->width;
    d_image->height = image->height;
    d_image->pixels = _alloc(image->width * image->height * sizeof(unsigned char));

    /* 
     * Create several SPE-threads to execute 'spu'.
     */
    int i;
    for(i = 0; i < spu_num; i++) {
        /* Thread arguments */
        thread_arg[i].spu = i;
        thread_arg[i].image = image;
        thread_arg[i].c_image = c_image;
        thread_arg[i].d_image = d_image;
        thread_arg[i].mode_vect = mode_vect;
        thread_arg[i].mode_dma = mode_dma;
        thread_arg[i].spu_num = spu_num;

        /* Create thread for each SPE context */
        if (pthread_create(&threads[i], NULL, &ppu_pthread_function, &thread_arg[i]))  {
            perror("Failed creating thread");
            exit(1);
        }
    }

    /* Wait for SPU-thread to complete execution.  */
    for (i = 0; i < spu_num; i++) {
        if (pthread_join(threads[i], NULL)) {
            perror("Failed pthread_join");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]){

    if (argc != 7){
        printf("Usage: %s mod_vect mod_dma num_spus in.pgm out.cmp out.pgm\n", argv[0]);
        return 0;
    }

    /* original and decompressed images */
    struct img image __attribute__ ((aligned(16)));
    struct img d_image __attribute__ ((aligned(16)));

    /* compressed image */
    struct c_img c_image __attribute__ ((aligned(16)));

    /* command line arguments */
    mode_vect_t mode_vect = atoi(argv[1]);
    mode_dma_t mode_dma = atoi(argv[2]);
    int num_spus = atoi(argv[3]);
    char *in_pgm = argv[4];
    char *out_cmp = argv[5];
    char *out_pgm = argv[6];

    /* time measurement */
    struct timeval t1, t2, t3, t4;
    double total_time = 0, scale_time = 0;

    /* start timer, including I/O */
    gettimeofday(&t3, NULL);    

    read_pgm(in_pgm, &image);  

    /* start timer, without I/O */
    gettimeofday(&t1, NULL);
    compress_parallel(&image, &c_image, &d_image, mode_vect, mode_dma, num_spus);

    /* stop timer */
    gettimeofday(&t2, NULL);    

    write_cmp(out_cmp, &c_image);
    write_pgm(out_pgm, &d_image);

    /* free resources */
    free_cmp(&c_image);
    free_pgm(&image);
    free_pgm(&d_image);

    /* stop timer */
    gettimeofday(&t4, NULL);

    total_time += GET_TIME_DELTA(t3, t4);
    scale_time += GET_TIME_DELTA(t1, t2);

    printf("Compress / Decompress time: %lf\n", scale_time);
    printf("Total time: %lf\n", total_time);

    return 0;
}   
