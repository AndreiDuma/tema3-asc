#include <stdio.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "../cmp.h"

#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
#define MAX_SPU_THREADS   8

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

    printf("%p\n", args.image);

    /* eliberam tag id-ul */
    mfc_tag_release(tag_id);

    return 0;
}
