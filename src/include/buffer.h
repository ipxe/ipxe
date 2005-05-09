#ifndef BUFFER_H
#define BUFFER_H

struct buffer_free_block {
	struct buffer_free_block *next;
	struct buffer_free_block *prev;	
	void *end;
};

struct buffer {
	struct buffer_free_block free_blocks;
	void *start;
	void *end;
};

#endif /* BUFFER_H */
