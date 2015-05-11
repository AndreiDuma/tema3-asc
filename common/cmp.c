#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmp.h"

void write_cmp(char* path, struct c_img* out_img){
	int i, nr_blocks, j, fd, k, file_size;
	char *buf, small_buf[BUF_SIZE];
	struct nibbles nb;
	
	fd = _open_for_write(path);
	
	//write width and height
	sprintf(small_buf, "%d\n%d\n", out_img->width, out_img->height);
	_write_buffer(fd, small_buf, strlen(small_buf));
	
	nr_blocks = out_img->width * out_img->height / (BLOCK_SIZE * BLOCK_SIZE);
	file_size = nr_blocks * (2 + BLOCK_SIZE * BLOCK_SIZE / 2);
	buf = _alloc(file_size);

	k = 0;
	for (i=0; i<nr_blocks; i++){
		//write min and max
		buf[k++] = out_img->blocks[i].min;
		buf[k++] = out_img->blocks[i].max;		
		//write index matrix
		j = 0;
		while (j < BLOCK_SIZE * BLOCK_SIZE){
			nb.first_nibble = out_img->blocks[i].index_matrix[j++];
			nb.second_nibble = out_img->blocks[i].index_matrix[j++];
			buf[k++] = *((char*) &nb);
		}
	}
	_write_buffer(fd, buf, file_size);

	free(buf);
	close(fd);
}

void read_cmp(char* path, struct c_img* out_img){	
	int fd, nr_blocks, i, j = 0, k, file_size;
	char *big_buf, small_buf[BUF_SIZE];
	struct nibbles nb;
	
	fd = _open_for_read(path);
	
	//read width and height
	read_line(fd, path, small_buf, BUF_SIZE);
	out_img->width = atoi(small_buf);
	read_line(fd, path, small_buf, BUF_SIZE);
	out_img->height = atoi(small_buf);
	
	nr_blocks = out_img->width * out_img->height / (BLOCK_SIZE * BLOCK_SIZE);
	out_img->blocks = (struct block*) _alloc(nr_blocks * sizeof(struct block));
	file_size = nr_blocks * (2 + BLOCK_SIZE * BLOCK_SIZE / 2);
	
	big_buf = (char*) _alloc(file_size);

	_read_buffer(fd, big_buf, file_size);

	for (i=0; i<nr_blocks; i++){
		//read a and b
		out_img->blocks[i].min = big_buf[j++];
		out_img->blocks[i].max = big_buf[j++];
		//read index_matrix
		k = 0;
		while (k < BLOCK_SIZE * BLOCK_SIZE){
			nb = *((struct nibbles*) &big_buf[j++]);
			out_img->blocks[i].index_matrix[k++] = nb.first_nibble;
			out_img->blocks[i].index_matrix[k++] = nb.second_nibble;
		}
	}
	free(big_buf);
	close(fd);
}

void free_cmp(struct c_img* image){
	free(image->blocks);
}
