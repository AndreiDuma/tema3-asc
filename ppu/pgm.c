#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmp.h"

void read_pgm(char* path, struct img* in_img){
	int fd;
	char buf[BUF_SIZE], *token;

	fd = _open_for_read(path);

	//read file type; expecting P5
	read_line(fd, path, buf, BUF_SIZE);
	if (strncmp(buf, "P5", 2)){
		fprintf(stderr, "Expected binary PGM (P5 type) when reading from %s\n", path);
		exit(0);
	}

	//read comment line
	read_line(fd, path, buf, BUF_SIZE);

	//read image width and height
	read_line(fd, path, buf, BUF_SIZE);
	token = strtok(buf, " ");
	if (token == NULL){
		fprintf(stderr, "Expected token when reading from %s\n", path);
		exit(0);
	}	
	in_img->width = atoi(token);
	token = strtok(NULL, " ");
	if (token == NULL){
		fprintf(stderr, "Expected token when reading from %s\n", path);
		exit(0);
	}
	in_img->height = atoi(token);
	if (in_img->width < 0 || in_img->height < 0){
		fprintf(stderr, "Invalid width or height when reading from %s\n", path);
		exit(0);
	}

	//read max value
	read_line(fd, path, buf, BUF_SIZE);

	in_img->pixels = (unsigned char*) _alloc(in_img->width * in_img->height *
			sizeof (unsigned char));
			
	_read_buffer(fd, in_img->pixels, in_img->width * in_img->height);

	close(fd);
}

void write_pgm(char* path, struct img* out_img){
	int fd; 
	char buf[BUF_SIZE];

	fd = _open_for_write(path);

	//write image type
	strcpy(buf, "P5\n");
	_write_buffer(fd, buf, strlen(buf));

	//write comment 
	strcpy(buf, "#Decompressed\n");
	_write_buffer(fd, buf, strlen(buf));

	//write image width and height
	sprintf(buf, "%d %d\n", out_img->width, out_img->height);
	_write_buffer(fd, buf, strlen(buf));

	//write max value
	strcpy(buf, "255\n");
	_write_buffer(fd, buf, strlen(buf));

	//write image pixels
	_write_buffer(fd, out_img->pixels, out_img->width * out_img->height);

	close(fd);
}

void free_pgm(struct img* image){
	free(image->pixels);
}
