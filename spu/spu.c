#include <stdio.h>
#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "../common/cmp.h"

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
    mfc_get(&args, argp, 16, tag_id, 0, 0);
    waitag(tag_id);

    /* cititi in Local Store un set de date din A si unul din B (vezi cerinta 3) */
    uint32_t left = CHUNK * sizeof(float), size, offset = 0;
    while (left > 0) {
        size = left;
        if (left > MAX) {
            size = MAX;
        }

        mfc_get((void *)(A) + offset, (uint32_t)(arg.A) + offset, size, tag_id, 0, 0);
        waitag(tag_id);
        mfc_get((void *)(B) + offset, (uint32_t)(arg.B) + offset, size, tag_id, 0, 0);
        waitag(tag_id);

        offset += size;
        left -= size;
    }

    /* adunati element cu element A si B folosind operatii vectoriale (vezi cerinta 4) */
    vector float *vA = (vector float *)A;
    vector float *vB = (vector float *)B;
    vector float *vC = (vector float *)C;

    uint32_t i;
    for (i = 0; i < CHUNK / sizeof(vector float); i++) {
        vC[i] = spu_add(vA[i], vB[i]);
    }

    /* scrieti in main storage un set de date din C (vezi cerinta 5) */
    /* repetati pasii de mai sus de cate ori este nevoie pentru a acoperi toate elementele (vezi cerinta 6) */
    left = CHUNK * sizeof(float);
    offset = 0;
    while (left > 0) {
        size = left;
        if (left > MAX) {
            size = MAX;
        }

        mfc_put((void *)(C) + offset, (uint32_t)(arg.C) + offset, size, tag_id, 0, 0);
        waitag(tag_id);

        offset += size;
        left -= size;
    }

    /* eliberam tag id-ul */
    mfc_tag_release(tag_id);

    return 0;
}
