#include <criterion/criterion.h>
#include <errno.h>
#include <signal.h>
#include "debug.h"
#include "sfmm.h"
#define TEST_TIMEOUT 15

/*
 * Assert the total number of free blocks of a specified size.
 * If size == 0, then assert the total number of all free blocks.
 */
void assert_free_block_count(size_t size, int count) {
    int cnt = 0;
    for(int i = 0; i < NUM_FREE_LISTS; i++) {
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	while(bp != &sf_free_list_heads[i]) {
	    if(size == 0 || size == (bp->header & ~0x1f))
		cnt++;
	    bp = bp->body.links.next;
	}
    }
    if(size == 0) {
	cr_assert_eq(cnt, count, "Wrong number of free blocks (exp=%d, found=%d)",
		     count, cnt);
    } else {
	cr_assert_eq(cnt, count, "Wrong number of free blocks of size %ld (exp=%d, found=%d)",
		     size, count, cnt);
    }
}

/*
 * Assert that the free list with a specified index has the specified number of
 * blocks in it.
 */
void assert_free_list_size(int index, int size) {
    int cnt = 0;
    sf_block *bp = sf_free_list_heads[index].body.links.next;
    while(bp != &sf_free_list_heads[index]) {
	cnt++;
	bp = bp->body.links.next;
    }
    cr_assert_eq(cnt, size, "Free list %d has wrong number of free blocks (exp=%d, found=%d)",
		 index, size, cnt);
}

Test(sfmm_basecode_suite, malloc_an_int, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	int *x = sf_malloc(sizeof(int));

	cr_assert_not_null(x, "x is NULL!");

	*x = 4;

	cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");

	assert_free_block_count(0, 1);
	assert_free_block_count(1952, 1);
	assert_free_list_size(7, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
	cr_assert(sf_mem_start() + PAGE_SZ == sf_mem_end(), "Allocated more than necessary!");
}

Test(sfmm_basecode_suite, malloc_four_pages, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;

	// We want to allocate up to exactly four pages.
	void *x = sf_malloc(16288);
	cr_assert_not_null(x, "x is NULL!");
	assert_free_block_count(0, 0);
	cr_assert(sf_errno == 0, "sf_errno is not 0!");
}

Test(sfmm_basecode_suite, malloc_too_large, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
        void *x = sf_malloc(PAGE_SZ * 100);

	cr_assert_null(x, "x is not NULL!");
	assert_free_block_count(0, 1);
	assert_free_block_count(36800, 1);
	cr_assert(sf_errno == ENOMEM, "sf_errno is not ENOMEM!");
}

Test(sfmm_basecode_suite, free_no_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	/* void *x = */ sf_malloc(8);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(1);

	sf_free(y);

	assert_free_block_count(0, 2);
	assert_free_block_count(224, 1);
	assert_free_block_count(1696, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, free_coalesce, .timeout = TEST_TIMEOUT) {
	sf_errno = 0;
	/* void *w = */ sf_malloc(8);
	void *x = sf_malloc(200);
	void *y = sf_malloc(300);
	/* void *z = */ sf_malloc(4);

	sf_free(y);
	sf_free(x);

	assert_free_block_count(0, 2);
	assert_free_block_count(544, 1);
	assert_free_block_count(1376, 1);

	cr_assert(sf_errno == 0, "sf_errno is not zero!");
}

Test(sfmm_basecode_suite, freelist, .timeout = TEST_TIMEOUT) {
	void *u = sf_malloc(200);
	/* void *v = */ sf_malloc(300);
	void *w = sf_malloc(200);
	/* void *x = */ sf_malloc(500);
	void *y = sf_malloc(200);
	/* void *z = */ sf_malloc(700);

	sf_free(u);
	sf_free(w);
	sf_free(y);

	assert_free_block_count(0, 4);
	assert_free_block_count(224, 3);
	assert_free_block_count(1760, 1);
	assert_free_list_size(4, 3);
	assert_free_list_size(7, 1);

	// First block in list should be the most recently freed block.
	int i = 4;
	sf_block *bp = sf_free_list_heads[i].body.links.next;
	cr_assert_eq(bp, (char *)y - sizeof(sf_header),
		     "Wrong first block in free list %d: (found=%p, exp=%p)",
                     i, bp, (char *)y - sizeof(sf_header));
}

Test(sfmm_basecode_suite, realloc_larger_block, .timeout = TEST_TIMEOUT) {
	void *x = sf_malloc(sizeof(int));
	/* void *y = */ sf_malloc(10);
	x = sf_realloc(x, sizeof(int) * 20);

	cr_assert_not_null(x, "x is NULL!");
	sf_block *bp = (sf_block *)((char *)x - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x1f) == 96, "Realloc'ed block size not what was expected!");

	assert_free_block_count(0, 2);
	assert_free_block_count(1824, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_splinter, .timeout = TEST_TIMEOUT) {
	void *x = sf_malloc(sizeof(int) * 20);
	void *y = sf_realloc(x, sizeof(int) * 16);

	cr_assert_not_null(y, "y is NULL!");
	cr_assert(x == y, "Payload addresses are different!");

	sf_block *bp = (sf_block *)((char*)y - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x1f) == 96, "Block size not what was expected!");

	// There should be only one free block.
	assert_free_block_count(0, 1);
	assert_free_block_count(1888, 1);
}

Test(sfmm_basecode_suite, realloc_smaller_block_free_block, .timeout = TEST_TIMEOUT) {
	void *x = sf_malloc(sizeof(double) * 8);
	void *y = sf_realloc(x, sizeof(int));

	cr_assert_not_null(y, "y is NULL!");

	sf_block *bp = (sf_block *)((char*)y - sizeof(sf_header));
	cr_assert(bp->header & THIS_BLOCK_ALLOCATED, "Allocated bit is not set!");
	cr_assert((bp->header & ~0x1f) == 32, "Realloc'ed block size not what was expected!");

	// After realloc'ing x, we can return a block of size 32 to the freelist.
	// This block will be coalesced.
	assert_free_block_count(0, 1);
	assert_free_block_count(1952, 1);
}

//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################

int getNeededSize(int inputSize, int alignment){
	inputSize += 16; //for footer and header
	while(inputSize % alignment != 0){
		inputSize++;
	}
	return inputSize;
}

//tests the equality of the footer and header for malloc
Test(sfmm_student_suite, student_test_1, .timeout = TEST_TIMEOUT) {
	void *x = sf_malloc(sizeof(int));
	sf_block *bp = (void*)x - 8;
	size_t size = bp->header & ~0x1F;
	sf_footer *bFoot= (void*)bp + size-8;

	cr_assert(bp->header == *bFoot, "Head and Foot do not match");
}
//tests coalesce more vigorously along with using the correct freed block 
Test(sfmm_student_suite, student_test_2, .timeout = TEST_TIMEOUT){
	void *x =  sf_malloc(8);
	void *y = sf_malloc(200);
	void *z =  sf_malloc(1);

	assert_free_block_count(1984-getNeededSize(1, 32)-getNeededSize(200, 32)- getNeededSize(8, 32), 1);//there is just the wilderness

	sf_free(x);

	assert_free_block_count(getNeededSize(8, 32), 1);//one freed original x block
	assert_free_block_count(1984-getNeededSize(1, 32)-getNeededSize(200, 32)-getNeededSize(8,32), 1);//one wilderness block
	assert_free_list_size(0, 1);//the first list has one element
	assert_free_list_size(7, 1);//one wilderness block

	sf_free(y);//this is a coalesce

	assert_free_block_count(getNeededSize(8, 32)+ getNeededSize(200, 32), 1);//there is one coalesced block
	assert_free_list_size(7, 1);//one wilderness block

	void *w = sf_malloc(8);//this is a spliter

	assert_free_block_count(getNeededSize(200, 32), 1);

	sf_free(w);//this is a right coalesce
	sf_free(z);//this is a full coalesce into the wilderness

	assert_free_block_count(1984, 1);
}
//checks the memalign if it is already aligned and if it is cut properly 
Test(sfmm_student_suite, student_test_3, .timeout = TEST_TIMEOUT){
	/*void *x  = */sf_memalign(8, 32);//this should be the equiv as malloc(8)
	assert_free_block_count(1984 - getNeededSize(8, 32), 1);

}
//checks proper alignment
Test(sfmm_student_suite, student_test_4, .timeout = TEST_TIMEOUT){
	void *x  = sf_memalign(8, 1024);//this should result in a block with size 32 but a payload pointer alligned on 1024
	cr_assert((uintptr_t)x % 1024 ==0, "The pointer is not 1024 byte aligned");

}

//checks memalign more indepth
Test(sfmm_student_suite, student_test_5, .timeout = TEST_TIMEOUT){
	void* y = sf_malloc(8);
	void *x  = sf_memalign(8, 1024);//this should result in a block with size 32 but a payload pointer alligned on 1024
	cr_assert((uintptr_t)x % 1024 ==0, "The pointer is not 1024 byte aligned");

	sf_free(x);

	sf_free(y);

	assert_free_block_count(1984, 1);//everything should be collesed back together
}

//stress test all functions
Test(sfmm_student_suite, student_test_6, .timeout = TEST_TIMEOUT){
	void* a = sf_malloc(8);
	void* b = sf_malloc(sizeof(long double));
	void* c = sf_malloc(8);
	void* e = sf_memalign(8, 64);

	cr_assert((uintptr_t)e % 64 ==0, "The pointer is not 64 byte aligned");

	*(int*)a = 1;
	*(long double*)b = 2;
	*(int*)c = 3;
	*(int*)e = 4;

	cr_assert(*(int*)a == 1, "a is not equal");
	cr_assert(*(long double*)b == 2, "b1 is not equal");
	cr_assert(*(int*)c == 3, "c1 is not equal");
	cr_assert(*(int*)e == 4, "e1 is not equal");

	void* d = sf_realloc(a, 16);
	*(int*)d = 5;

	cr_assert(*(int*)d == 5, "d is not equal");
	cr_assert(*(long double*)b == 2, "b2 is not equal");
	cr_assert(*(int*)c == 3, "c2 is not equal");
	cr_assert(*(int*)e == 4, "e2 is not equal");

	sf_free(e);
	sf_free(c);
	sf_free(b);
	sf_free(d);
	
	for(int i=0; i<7; i++){
		assert_free_list_size(i, 0);
	}
	assert_free_block_count(1984, 1);
}