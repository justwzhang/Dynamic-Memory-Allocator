#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {
    // double* ptr = sf_malloc(sizeof(double));
    // if (ptr == NULL){
    //     printf("%s\n", "NULL");
    // }


    // *ptr = 4;

    // double* ptr2 = sf_malloc(10);
    // printf("%f\n", *ptr);
    // printf("%p\n", ptr+32-8);
    // sf_block* block =(void*) ptr -8; 
    // sf_block* block2 =(void*) ptr2 -8; 
    // sf_show_block(block);
    // sf_show_block(block2);
    // printf("%s\n", "blocks");
    // sf_show_blocks();
    // printf("%s\n", "free lists");
    // sf_show_free_lists();
    // printf("\n" );
    // sf_show_heap();
    // sf_free(ptr);
    // sf_show_heap();
    
    // printf("%p\n", ptr2);
    // sf_show_heap();


    //a test 
    // void *x =  sf_malloc(8);
    // // printf("%s\n", );
    // printf("%p\n", x);
    // // sf_show_heap();
    // void *y = sf_malloc(200);
    // // sf_show_heap();
    
    // void *z =  sf_malloc(1);
    // sf_free(x);
    // sf_show_heap();
    // printf("%s\n", "Free X-----------------------------------------------------------------FreeY");
    
    // sf_free(y);
    // sf_show_heap();
    // printf("%s\n", " Free Y-----------------------------------------------------------------malloc w");
    // void *w = sf_malloc(8);
    // printf("%p\n", w);
    // sf_show_heap();
    // printf("%s\n", "malloc w----------------------------------------------------------------- free w");
    // sf_free(w);
    // sf_show_heap();
    // printf("%s\n", "free w-----------------------------------------------------------------free z");
    // sf_free(z);
    // sf_show_heap();

    //from tests 
    //tests realloc
    // //works
    // //splinter 
    // void *w = sf_malloc(sizeof(int) * 20);
    // sf_show_heap();
    // printf("%s\n", "malloc w-----------------------------------------------------------------realloc z");
    // void *z = sf_realloc(w, sizeof(int) * 16);
    // sf_show_heap();
    // sf_free(z);


    // no splinter
    // void *x = sf_malloc(sizeof(double) * 8);
    // sf_block *block = x - 8;
    // printf("x size: %ld\n", block->header& ~0x1F);
    // sf_show_heap();
    // printf("%s\n", "malloc x-----------------------------------------------------------------realloc y");
    // void *y = sf_realloc(x, sizeof(int));
    // sf_block *block2 = y - 8;
    // printf("y size: %ld\n", block2->header& ~0x1F);
    // sf_show_heap();
    // sf_free(y);


    // void* x = sf_memalign(8, 1024);
    // sf_show_heap();
    // sf_free(x);



    // tests memalign
    // void* y = sf_malloc(8);
    // sf_show_heap();
    // printf("%s\n", "malloc y -------------------------------------------------------------------- memalign x");
    // void* x = sf_memalign(8, 1024);
    // sf_show_heap();
    // printf("%s\n", "memalign x -------------------------------------------------------------------- free x");
    

    // sf_free(x);
    // sf_show_heap();
    // printf("%s\n", "free x -------------------------------------------------------------------- free y");
    
    // sf_free(y);
    // sf_show_heap();

    // void* a = sf_malloc(8);
    // void* b = sf_malloc(200);
    // void* c = sf_malloc(8);
    // void* d = sf_realloc(a, 16);
    // void* e = sf_memalign(8, 64);
    // sf_show_heap();
    // printf("%s\n", "malloc -------------------------------------------------------------------- free e");
    // sf_free(e);

    // // sf_show_heap();
    // // printf("%s\n", "free e -------------------------------------------------------------------- free a");
    // // sf_free(a);

    // sf_show_heap();
    // printf("%s\n", "free a -------------------------------------------------------------------- free c");
    // sf_free(c);

    // sf_show_heap();
    // printf("%s\n", "free c -------------------------------------------------------------------- free b");
    // sf_free(b);

    // sf_show_heap();
    // printf("%s\n", "free b -------------------------------------------------------------------- free d");
    // sf_free(d);
    // sf_show_heap();

    void *y = sf_malloc(8);
    sf_show_heap();
    printf("%s\n", "-------------------------------------------------------------------------");
    void *x = sf_malloc(2048);
    sf_show_heap();
    printf("%s\n", "-------------------------------------------------------------------------");
    sf_free(y);
    sf_show_heap();
    printf("%s\n", "-------------------------------------------------------------------------");
    sf_free(x);
    sf_show_heap();

    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */
