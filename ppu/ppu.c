#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <libspe2.h>
#include <pthread.h>

#include "../cmp.h"

extern spe_program_handle_t spu;

#define MAX_SPU_THREADS   8
#define ARR_SIZE    80000
#define CHUNK (ARR_SIZE / MAX_SPU_THREADS)

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

    /* Run SPE context */
    unsigned int entry = SPE_DEFAULT_ENTRY;
    /* transfer prin argument adresa pentru transferul DMA initial */
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

void compress_serial(struct img *image, struct c_img *c_image) {
    int row, col, bl_row, bl_col, bl_index, nr_blocks;
    unsigned char min, max;
    float aux, factor;
    struct block *curr_block;

    c_image->width = image->width;
    c_image->height = image->height;
    nr_blocks = image->width * image->height / (BLOCK_SIZE * BLOCK_SIZE);

    c_image->blocks = (struct block*) _alloc(nr_blocks * sizeof(struct block));

    bl_index = 0;
    for (bl_row = 0; bl_row < image->height; bl_row += BLOCK_SIZE){
        for (bl_col = 0; bl_col < image->width; bl_col += BLOCK_SIZE){
            //process 1 block from input image

            curr_block = &c_image->blocks[bl_index];
            //compute min and max
            min = max = image->pixels[bl_row * image->width + bl_col];
            for (row = bl_row; row < bl_row + BLOCK_SIZE; row++){
                for (col = bl_col; col < bl_col + BLOCK_SIZE; col++){
                    if (image->pixels[row * image->width + col] < min)
                        min = image->pixels[row * image->width + col];
                    if (image->pixels[row * image->width + col] > max)
                        max = image->pixels[row * image->width + col];
                }
            }
            curr_block->min = min;
            curr_block->max = max;

            //compute factor
            factor = (max - min) / (float) (NUM_COLORS_PALETTE - 1);

            //compute index matrix
            if (factor != 0) {
                //min != max
                for (row = bl_row; row < bl_row + BLOCK_SIZE; row++){
                    for (col = bl_col; col < bl_col + BLOCK_SIZE; col++){
                        aux =  (image->pixels[row * image->width + col] - min) / factor;
                        curr_block->index_matrix[(row - bl_row) * BLOCK_SIZE + col - bl_col] = 
                            (unsigned char) (aux + 0.5);
                    }
                }
            } else {
                // min == max
                // all colors represented with min => index = 0
                memset(curr_block->index_matrix, 0, BLOCK_SIZE * BLOCK_SIZE);
            }

            bl_index++;
        }
    }

}

void compress_parallel(struct img *image, struct c_img *c_image, struct img *d_image, mode_vect_t mode_vect, mode_dma_t mode_dma, int spu_threads) {

    pthread_t threads[MAX_SPU_THREADS];
    args_t thread_arg[MAX_SPU_THREADS] __attribute__ ((aligned(16)));

    /* 
     * Create several SPE-threads to execute 'spu'.
     */
    int i;
    for(i = 0; i < spu_threads; i++) {
        /* Thread arguments */
        thread_arg[i].image = image;
        thread_arg[i].c_image = c_image;
        thread_arg[i].d_image = d_image;
        thread_arg[i].mode_vect = mode_vect;
        thread_arg[i].mode_dma = mode_dma;

        /* Create thread for each SPE context */
        if (pthread_create(&threads[i], NULL, &ppu_pthread_function, &thread_arg[i]))  {
            perror("Failed creating thread");
            exit(1);
        }
    }

    /* Wait for SPU-thread to complete execution.  */
    for (i = 0; i < spu_threads; i++) {
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

    /* Original and decompressed images */
    struct img image __attribute__ ((aligned(16)));
    struct img d_image __attribute__ ((aligned(16)));

    /* Compressed image */
    struct c_img c_image __attribute__ ((aligned(16)));

    /* Command line arguments */
    mode_vect_t mode_vect = atoi(argv[1]);
    mode_dma_t mode_dma = atoi(argv[2]);
    int num_spus = atoi(argv[3]);
    char *in_pgm = argv[4];
    char *out_cmp = argv[5];
    char *out_pgm = argv[6];

    /* Time measurement */
    struct timeval t1, t2, t3, t4;
    double total_time = 0, scale_time = 0;

    /* start timer, including I/O */
    gettimeofday(&t3, NULL);    

    read_pgm(in_pgm, &image);  

    /* Start timer, without I/O */
    gettimeofday(&t1, NULL);
    compress_parallel(&image, &c_image, &d_image, mode_vect, mode_dma, num_spus);

    //decompress_parallel(&d_image, &c_image);

    /* Stop timer */
    gettimeofday(&t2, NULL);    

    write_cmp(out_cmp, &c_image);
    write_pgm(out_pgm, &d_image);

    free_cmp(&c_image);
    free_pgm(&image);
    free_pgm(&d_image);

    /* Stop timer */
    gettimeofday(&t4, NULL);

    total_time += GET_TIME_DELTA(t3, t4);
    scale_time += GET_TIME_DELTA(t1, t2);

    printf("Compress / Decompress time: %lf\n", scale_time);
    printf("Total time: %lf\n", total_time);

    return 0;
}   
