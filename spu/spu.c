#include <stdio.h>
#include <string.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "../cmp.h"

#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
#define MAX_SPU_THREADS   8

void blk_print(unsigned char *pixels);
void compress(struct args *arg, uint32_t tag_id);
void decompress(struct args *arg, uint32_t tag_id);;

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp) {
    uint32_t tag_id = mfc_tag_reserve();
    if (tag_id==MFC_TAG_INVALID){
        printf("SPU: ERROR can't allocate tag ID\n");
        return -1;
    }

    struct args arg __attribute__ ((aligned(16)));

    /* get the arguments through DMA */
    mfc_get((void *) &arg, (uint32_t) argp, sizeof(struct args), tag_id, 0, 0);
    waitag(tag_id);

    /* compress or decompress depending on mode_op parameter */
    switch (arg.mode_op) {
        case MODE_COMP:
            compress(&arg, tag_id);
            break;

        case MODE_DECOMP:
            decompress(&arg, tag_id);
            break;
    }

    /* release tag */
    mfc_tag_release(tag_id);

    return 0;
}

void compress(struct args *arg, uint32_t tag_id) {
    /* Get image information */
    struct img image __attribute__ ((aligned(16)));
    struct c_img c_image __attribute__ ((aligned(16)));
    mfc_get((void *) &image, (uint32_t) arg->image, sizeof(struct img), tag_id, 0, 0);
    mfc_get((void *) &c_image, (uint32_t) arg->c_image, sizeof(struct c_img), tag_id, 0, 0);
    waitag(tag_id);

    /* local pixel block */
    unsigned char pixels[BLOCK_SIZE * BLOCK_SIZE] __attribute__ ((aligned(16)));
    struct block blk __attribute__ ((aligned(16)));

    /* dimensions in blocks and number of blocks */
    int blk_width = image.width / BLOCK_SIZE,
        blk_height = image.height / BLOCK_SIZE,
        blk_num = blk_width * blk_height,
        blk_index;

    for (blk_index = arg->spu; blk_index < blk_num; blk_index += arg->spu_num) {
        /* start row and column of current block */
        int blk_row = blk_index / blk_width * BLOCK_SIZE,
            blk_col = blk_index % blk_width * BLOCK_SIZE;

        /* get block pixels through DMA */
        int row, col;
        for (row = 0; row < BLOCK_SIZE; row++) {
            unsigned char *ppu_pixel_row = image.pixels + (blk_row + row) * image.width + blk_col,
                          *spu_pixel_row = pixels + row * BLOCK_SIZE;
            mfc_get((void *) spu_pixel_row, (uint32_t) ppu_pixel_row, BLOCK_SIZE, tag_id, 0, 0);
        }
        waitag(tag_id);

        /* compute minimum and maximum */
        unsigned char min = pixels[0],
                      max = pixels[0];
        unsigned int i;
        for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++){
            if (pixels[i] < min) {
                min = pixels[i];
            }
            if (pixels[i] > max) {
                max = pixels[i];
            }
        }
        blk.min = min;
        blk.max = max;

        /* compute factor */
        float factor = (max - min) / (float) (NUM_COLORS_PALETTE - 1);

        /* compute index matrix */
        if (min != max) {

            /* computation mode */
            switch (arg->mode_vect) {
                case SCALAR:
                    for (row = 0; row < BLOCK_SIZE; row++){
                        for (col = 0; col < BLOCK_SIZE; col++){
                            float aux = (pixels[row * BLOCK_SIZE + col] - min) / factor;
                            blk.index_matrix[row * BLOCK_SIZE + col] = (unsigned char) (aux + 0.5);
                        }
                    }
                    break;

                case VECT: {
                    /* temporary buffer */
                    float fpixels[BLOCK_SIZE * BLOCK_SIZE] __attribute__ ((aligned(16)));
                    for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                        fpixels[i] = pixels[i];
                    }

                    vector float *vpixels = (vector float *) fpixels;
                    vector float vfactor = spu_splats(1 / factor),
                                 vmin = spu_splats((float) min),
                                 vhalf = spu_splats(0.5f);

                    /* compute indices */
                    for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE * sizeof(float) / sizeof(vector float); i++) {
                        vpixels[i] = spu_sub(vpixels[i], vmin);
                        vpixels[i] = spu_madd(vpixels[i], vfactor, vhalf);
                    }

                    for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                        blk.index_matrix[i] = (unsigned char) fpixels[i];
                    }

                    break;
                }

                case VECT_INTR: {
                    /* auxiliary pixel matrix */
                    unsigned int fpixels[BLOCK_SIZE * BLOCK_SIZE] __attribute__ ((aligned(16)));
                    memset(fpixels, 0, BLOCK_SIZE * BLOCK_SIZE * sizeof(unsigned int));

                    /* convert unsigned char to unsigned int */
                    unsigned char *cpixels = (unsigned char *) fpixels;
                    for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                        cpixels[i * sizeof(unsigned int) + 3] = pixels[i];
                    }

                    vector float *vpixels = (vector float *) fpixels;
                    vector unsigned int *ipixels = (vector unsigned int *) fpixels;

                    vector float vfactor = spu_splats(1 / factor),
                                 vmin = spu_splats((float) min),
                                 vhalf = spu_splats(0.5f);

                    /* compute indices */
                    for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE * sizeof(float) / sizeof(vector float); i++) {
                        /* convert unsigned integer to float */
                        vpixels[i] = spu_convtf(ipixels[i], 0);

                        vpixels[i] = spu_sub(vpixels[i], vmin);
                        vpixels[i] = spu_madd(vpixels[i], vfactor, vhalf);

                        /* convert float to unsigned int */
                        ipixels[i] = spu_convtu(vpixels[i], 0);
                    }

                    /* back to unsigned char */
                    for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                        blk.index_matrix[i] = cpixels[i * sizeof(unsigned int) + 3];
                    }

                    break;
                }
            }
        } else {
            /* all colors represented with min => index = 0 */
            memset(blk.index_matrix, 0, BLOCK_SIZE * BLOCK_SIZE);
        }

        /* put the block back in memory */
        struct block *ppu_blk = c_image.blocks + blk_index;
        mfc_put((void *) &blk, (uint32_t) ppu_blk, sizeof(struct block), tag_id, 0, 0);
        waitag(tag_id);
    }
}

void decompress(struct args *arg, uint32_t tag_id) {
    /* Get image information */
    struct c_img c_image __attribute__ ((aligned(16)));
    struct img d_image __attribute__ ((aligned(16)));
    mfc_get((void *) &c_image, (uint32_t) arg->c_image, sizeof(struct c_img), tag_id, 0, 0);
    mfc_get((void *) &d_image, (uint32_t) arg->d_image, sizeof(struct img), tag_id, 0, 0);
    waitag(tag_id);

    /* local pixel block */
    struct block blk;
    unsigned char pixels[BLOCK_SIZE * BLOCK_SIZE];

    /* dimensions in blocks and number of blocks */
    int blk_width = c_image.width / BLOCK_SIZE,
        blk_height = c_image.height / BLOCK_SIZE,
        blk_num = blk_width * blk_height,
        blk_index;

    for (blk_index = arg->spu; blk_index < blk_num; blk_index += arg->spu_num) {
        /* start row and column of current block */
        int blk_row = blk_index / blk_width * BLOCK_SIZE,
            blk_col = blk_index % blk_width * BLOCK_SIZE;

        /* get block through DMA */
        struct block *ppu_blk = c_image.blocks + blk_index;
        mfc_get((void *) &blk, (uint32_t) ppu_blk, sizeof(struct block), tag_id, 0, 0);
        waitag(tag_id);

        /* compute factor */
        float factor = (blk.max - blk.min) / (float) (NUM_COLORS_PALETTE - 1);

        /* restore pixels by requested method */
        switch (arg->mode_vect) {
            case SCALAR: {
                unsigned int i;
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++){
                    pixels[i] = (unsigned char) (blk.min + blk.index_matrix[i] * factor + 0.5);
                }

                break;
            }

            case VECT: {
                /* temporary buffer */
                float aux[BLOCK_SIZE * BLOCK_SIZE];
                unsigned int i;

                /* cast to float */
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                    aux[i] = (float) blk.index_matrix[i];
                }

                vector float vfactor = spu_splats(factor),
                             vminhalf = spu_splats((float) blk.min + 0.5f);

                vector float *vaux = (vector float *) aux;

                /* compute pixels */
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE * sizeof(float) / sizeof(vector float); i++) {
                    vaux[i] = spu_madd(vaux[i], vfactor, vminhalf);
                }

                /* cast back to unsigned char */
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                    pixels[i] = (unsigned char) aux[i];
                }

                break;
            }

            case VECT_INTR: {
                /* auxiliary pixel matrix */
                unsigned int aux[BLOCK_SIZE * BLOCK_SIZE];
                memset(aux, 0, BLOCK_SIZE * BLOCK_SIZE * sizeof(unsigned int));

                /* convert from unsigned char to unsigned int */
                unsigned char *caux = (unsigned char *) aux;
                unsigned int i;
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                    caux[i * sizeof(unsigned int) + 3] = blk.index_matrix[i];
                }

                vector float vfactor = spu_splats(factor),
                             vminhalf = spu_splats((float) blk.min + 0.5f);

                vector float *vaux = (vector float *) aux;
                vector unsigned int *ivaux = (vector unsigned int *) aux;

                /* compute pixels */
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE * sizeof(float) / sizeof(vector float); i++) {
                    /* convert unsigned int to float */
                    vaux[i] = spu_convtf(ivaux[i], 0);

                    vaux[i] = spu_madd(vaux[i], vfactor, vminhalf);

                    /* convert float to unsigned int */
                    ivaux[i] = spu_convtu(vaux[i], 0);
                }

                /* back to unsigned char */
                for (i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++) {
                    pixels[i] = caux[i * sizeof(unsigned int) + 3];
                }

                break;
            }
        }

        /* put the pixels back in memory */
        int row;
        for (row = 0; row < BLOCK_SIZE; row++) {
            unsigned char *ppu_pixel_row = d_image.pixels + (blk_row + row) * d_image.width + blk_col,
                          *spu_pixel_row = pixels + row * BLOCK_SIZE;
            mfc_put((void *) spu_pixel_row, (uint32_t) ppu_pixel_row, BLOCK_SIZE, tag_id, 0, 0);
        }
        waitag(tag_id);
    }
}

/**
 * Prints a BLOCK_SIZE x BLOCK_SIZE block of pixels.
 */
void blk_print(unsigned char *pixels) {
    int row, col;
    for (row = 0; row < BLOCK_SIZE; row++) {
        for (col = 0; col < BLOCK_SIZE; col++) {
            printf("%4d", pixels[row * BLOCK_SIZE + col]);
        }
        printf("\n");
    }
    printf("\n");
}
