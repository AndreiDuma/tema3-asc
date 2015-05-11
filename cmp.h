#ifndef __CMP_H
#define __CMP_H

#define BUF_SIZE 		256
#define BLOCK_SIZE 		16
#define NUM_COLORS_PALETTE 16

//macro for easily getting how much time has passed between two events
#define GET_TIME_DELTA(t1, t2) ((t2).tv_sec - (t1).tv_sec + \
		((t2).tv_usec - (t1).tv_usec) / 1000000.0)

/* Arguments structures */
typedef enum {
    SCALAR,
    VECT,
    VECT_INTR
} mode_vect_t;

typedef enum {
    DMA,
    DMA_DOUBLE_BUF
} mode_dma_t;

struct args {
    struct img *image;
    struct c_img *c_image;
    struct img *d_image;

    mode_vect_t mode_vect;
    mode_dma_t mode_dma;
} __attribute__ ((aligned(16)));

typedef struct args args_t;

/* Image structures */
struct img {
	//regular image
	int width, height;
	unsigned char* pixels;
};

struct block {
	//min and max values for the block
	unsigned char min, max;
	//index matrix for the pixels in the block
	unsigned char index_matrix[BLOCK_SIZE * BLOCK_SIZE];
} __attribute__ ((aligned(16))) args_t;

struct c_img{
	//compressed image
	int width, height;
	struct block* blocks;
};

struct nibbles {
	unsigned first_nibble : 4;
	unsigned second_nibble: 4;
};

//utils
void* _alloc(int size);
void _read_buffer(int fd, void* buf, int size);
void _write_buffer(int fd, void* buf, int size);
int _open_for_write(char* path);
int _open_for_read(char* path);
void read_line(int fd, char* path, char* buf, int buf_size);
//read_cmp
void read_cmp(char* path, struct c_img* out_img);
void write_cmp(char* path, struct c_img* out_img);
void free_cmp(struct c_img* out_img);
//read_pgm
void read_pgm(char* path, struct img* in_img);
void write_pgm(char* path, struct img* out_img);
void free_pgm(struct img* out_img);

#endif
