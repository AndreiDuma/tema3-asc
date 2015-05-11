#include <stdio.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "../cmp.h"

#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
#define MAX_SPU_THREADS   8

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

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp)
{
    uint32_t tag_id = mfc_tag_reserve();
    if (tag_id==MFC_TAG_INVALID){
        printf("SPU: ERROR can't allocate tag ID\n"); 
        return -1;
    }

    args_t args __attribute__ ((aligned(16)));

    /* Get the arguments through DMA */
    mfc_get((void *) &args, (uint32_t) argp, sizeof(args_t), tag_id, 0, 0);
    waitag(tag_id);

    /* Get image information */
    struct img image;
    struct c_img c_image;
    struct img d_image;
    mfc_get((void *) &image, (uint32_t) args.image, sizeof(struct img), tag_id, 0, 0);
    waitag(tag_id);

    unsigned char pixels[BLOCK_SIZE * BLOCK_SIZE];
    struct block blk;


    /* eliberam tag id-ul */
    mfc_tag_release(tag_id);

    return 0;
}
