/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

//#define NUM_FREE_LISTS 8
//This is the number of total free lists

// struct sf_block sf_free_list_heads[NUM_FREE_LISTS];
// M = 32
// sf_free_list_heads[0]: M
// sf_free_list_heads[1]: 2M
// sf_free_list_heads[2]: 3M
// sf_free_list_heads[3]: (3M, 5M]
// sf_free_list_heads[4]: (5M, 8M]
// sf_free_list_heads[5]: (8M, 13M]
// sf_free_list_heads[6]: >13M
// sf_free_list_heads[7]: Wilderness block

int alignment = 32; //32 bit or 4 bytes use as M multiplier

int sf_Start = 0; //to see if we need to init the list heads

int headSize = 8; //8 bytes or 64 bit

struct sf_block *prol;//the prologue which doesnt change
struct sf_block *epil;//the epilogue which can change

//inserts the newly freed block to the start of the list at sf_free_list_heads[index]
void insertToList(int index, sf_block* block){
    struct sf_block* sentinel = &sf_free_list_heads[index];
    struct sf_block* first = sentinel->body.links.next;
    sentinel->body.links.next = block;
    block->body.links.prev = sentinel;
    block->body.links.next = first;
    first->body.links.prev = block;
}

int getListIndex(int sizeNeeded){
    int multiplier = sizeNeeded/32;
    int startIndex = 0;
    if(multiplier == 2){
        startIndex = 1;
    }else if(multiplier == 3){
        startIndex = 2;
    }else if((multiplier == 4 )|| (multiplier == 5)){
        startIndex = 3;
    }else if((multiplier == 6) || (multiplier==7)|| (multiplier==8)){
        startIndex = 4;
    }else if((multiplier == 9)||(multiplier == 10)|| (multiplier==11) || (multiplier == 12) ||(multiplier ==13)){
        startIndex = 5;
    }else if(multiplier >13){
        startIndex = 6;
    }
    return startIndex;
}

void* initWilderness(){
    void* newPage = sf_mem_grow();
    if(newPage == NULL){// if there was an error with growing the heap
        sf_errno = ENOMEM;
        return NULL;
    }
    int paddingCount = 0;
    while((uintptr_t)(newPage + 8) % 32 != 0){//add the padding to the new page if the 
        newPage += 1; //add a "row"
        paddingCount += 1;
    }
    //newPage should now be pointing to the first header alligned address

    prol = newPage;//set the prologue
    sf_header prolHead= 32;
    prol->header = prolHead | THIS_BLOCK_ALLOCATED;

    epil = sf_mem_end() - 8;//set the epilogue

    sf_header* epilHead = (sf_header*)epil;
    *epilHead = THIS_BLOCK_ALLOCATED;

    newPage += 32;// add 32 bytes (32 is the smallest block)
    sf_footer *prolFooter = newPage-8;
    sf_footer prolFoot = 0 | 32;
    memcpy(prolFooter, &prolFoot, 8);//set the prol footer

    //newPage should be pointing to the next header start
    struct sf_block *tempStartingBlock = newPage;

    int sent7Size = 2048-32-paddingCount - 8; //total -prol- the padding bytes - 8 bytes for the epilogue = size of the whole block
    sf_header sent7head = 0 | sent7Size;//set the size
    tempStartingBlock->header = sent7head;

    sf_footer *tempFooterPointer = (void*)tempStartingBlock + sent7Size- 8;//the start of the block including head plus the size - 8 = footer start byte
    memcpy(tempFooterPointer, &sent7head, 8);//create the footer

    //set the next and prev
    tempStartingBlock->body.links.next = &sf_free_list_heads[7];//set the sentinel block to the next and prev of the block
    tempStartingBlock->body.links.prev = &sf_free_list_heads[7];
    sf_free_list_heads[7].body.links.next = tempStartingBlock;//set the next to the contents of the block
    sf_free_list_heads[7].body.links.prev = tempStartingBlock;//set the prev to the contents of the block
    return newPage;
}
/*
 * This is your implementation of sf_malloc. It acquires uninitialized memory that
 * is aligned and padded properly for the underlying system.
 *
 * @param size The number of bytes requested to be allocated.
 *
 * @return If size is 0, then NULL is returned without setting sf_errno.
 * If size is nonzero, then if the allocation is successful a pointer to a valid region of
 * memory of the requested size is returned.  If the allocation is not successful, then
 * NULL is returned and sf_errno is set to ENOMEM.
 */
void *sf_malloc(size_t size) {//size is in bytes
    if (size == 0){//invalid size
        return NULL;
    }
    //set up the wilderness block along with the rest of the free list heads sentinels 
    if(sf_Start == 0){//need to setup the free lists and allocate first block to wilderness block
        for(int i=0; i<NUM_FREE_LISTS; i++){
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        void* temp = initWilderness();
        if (temp == NULL){
            return NULL;
        }

        //increments sf_start so regular mem allocation can take place
        sf_Start = 1;
    }
    if((sf_free_list_heads[7].body.links.next) == (&sf_free_list_heads[7])){
        void* temp = initWilderness();
        if (temp == NULL){
            return NULL;
        }
    }
    // 8 for header and 8 for footer
    size_t blockSize = size + 16;
    int sizeNeeded = blockSize;
    //figure out the multiplier
    //use this to determine which list to start with
    while(sizeNeeded % 32 != 0){
        sizeNeeded++;
    }
    int startIndex = getListIndex(sizeNeeded);

    sf_header newhead = 0;
    newhead |= sizeNeeded;
    newhead |= THIS_BLOCK_ALLOCATED;
    void *returnPointer = NULL;

    for(int i =startIndex; i<8; i++){//iterate starting from the lowest starting index up to 7
        // printf("index %d\n", i);
        struct sf_block* sentinel = &sf_free_list_heads[i];//assign the sentinel of that index
        struct sf_block* current = sentinel->body.links.next;//assign the current looked at block
        // printf("Current: %p Sent: %p\n",current, sentinel );
        while(current != sentinel){//while searching through the circularly linked list
            // printf("Current: %p Sent: %p\n",current, sentinel );
            //the size of that block
            int sizeOfCurrentBlock = (current->header&~0x1F);
            //this i==7 if works
            if((i == 7) && sizeOfCurrentBlock >=  sizeNeeded){//break the wilderness block into one alocated block and one still un allocated block
                //create an allocated section that has size sizeNeeded
                current->header = newhead;
                sf_footer* newFoot =(void*)current + sizeNeeded - 8;
                memcpy(newFoot, &newhead, 8);//generate the new foot
                // sf_show_block(current);
                returnPointer = current->body.payload;//points to payload return this
                //now need to refactorthe remaining wilderness block
                if(sizeOfCurrentBlock-sizeNeeded>0){
                    sf_block* newWild = (void*)newFoot + 8;//points to the new head
                    sf_header newWildHead= sizeOfCurrentBlock - sizeNeeded;
                    newWild->header = newWildHead;
                    sf_footer* newWildFoot = (void*)newWild + (sizeOfCurrentBlock-sizeNeeded)-8;
                    memcpy(newWildFoot, &newWildHead, 8);
                    sf_free_list_heads[7].body.links.next = newWild;
                    sf_free_list_heads[7].body.links.prev = newWild;
                    newWild->body.links.next = &sf_free_list_heads[7];
                    newWild->body.links.prev = &sf_free_list_heads[7];
                }else{
                    sf_free_list_heads[7].body.links.next = &sf_free_list_heads[7];
                    sf_free_list_heads[7].body.links.prev = &sf_free_list_heads[7];
                }
                i = 8;
                break;
            }else if(i == 7){
                //get new page and concat it wil the previous
                void*newPage =sf_mem_grow();
                if(newPage == NULL){// if there was an error with growing the heap
                    sf_errno = ENOMEM;
                    return NULL;
                }
                epil = sf_mem_end() - 8;//set new epilouge

                sf_header* epilHead = (sf_header*)epil;
                *epilHead = THIS_BLOCK_ALLOCATED;

                current->header = sizeOfCurrentBlock+2048;
                sf_footer* newWildFoot = (void*)current + sizeOfCurrentBlock+2048-8;
                sf_footer newWildFooter= sizeOfCurrentBlock+2048-8;
                memcpy(newWildFoot, &newWildFooter, 8);
                sf_free_list_heads[7].body.links.next = current;
                sf_free_list_heads[7].body.links.prev = current;
                current->body.links.next = &sf_free_list_heads[7];
                current->body.links.prev = &sf_free_list_heads[7];

                i = startIndex; //restart now that memgrow was called
                break;
            }

//TODO TEST THIS IF STATEMENT
            if((i != 7) && (sizeOfCurrentBlock >=  sizeNeeded)){//needs testing but need to get free working first
                if(sizeNeeded == sizeOfCurrentBlock){// no need to split the block
                    sf_block* prev = current->body.links.prev;
                    sf_block* next = current->body.links.next;
                    //splice the list at this point and reconnect using the next and prev pointers
                    prev->body.links.next = next;
                    next->body.links.prev = prev;
                    sf_header newReturnHead= current->header | THIS_BLOCK_ALLOCATED;
                    current->header = newReturnHead;
                    sf_footer* newReturnFooter = (void*)current + sizeNeeded-8;
                    memcpy(newReturnFooter, &newReturnHead, 8);
                    returnPointer = current->body.payload;
                    i = 8;
                    break;
                }else{
                    //need to split the block because the size of this block is larger than the size needed so we must 
                    //splice it to sizeNeeded
                    sf_block* prev = current->body.links.prev;
                    sf_block* next = current->body.links.next;
                    prev->body.links.next = next;
                    next->body.links.prev = prev;
                    if(!((sizeOfCurrentBlock - sizeNeeded)<=32)){
                        sf_header newReturnHeader= sizeNeeded | THIS_BLOCK_ALLOCATED;
                        current->header = newReturnHeader;
                        sf_footer* newReturnFooter = (void*)current +sizeNeeded-8;
                        memcpy(newReturnFooter, &newReturnHeader, 8);
                        returnPointer = current->body.payload;
                        i=8;
                        //now need to create new header for the second part and 
                        sf_block *newBlock = (void*)current + sizeNeeded;
                        sf_header newBlkHeader= sizeOfCurrentBlock-sizeNeeded;
                        newBlock->header = newBlkHeader;
                        sf_footer *newBlkFooter = (void*)newBlock + sizeOfCurrentBlock-sizeNeeded-8;
                        memcpy(newBlkFooter, &newBlkHeader, 8);

                        //set this new block into the right list
                        int index= getListIndex(sizeOfCurrentBlock-sizeNeeded);
                        sf_block* originalNext = sf_free_list_heads[index].body.links.next;
                        sf_free_list_heads[index].body.links.next = newBlock;
                        newBlock->body.links.prev = &sf_free_list_heads[index];
                        newBlock->body.links.next = originalNext;
                        originalNext->body.links.prev = newBlock;
                        break;

                    }else{
                        sf_header newReturnHeader= current->header | THIS_BLOCK_ALLOCATED;
                        current->header = newReturnHeader;
                        sf_footer* newReturnFooter = (void*)current +sizeOfCurrentBlock-8;
                        memcpy(newReturnFooter, &newReturnHeader, 8);
                        returnPointer = current->body.payload;
                        i=8;
                        break;
                    }

                }

            //use this block
            }else{
                current = current->body.links.next;
            }
        }
    }

    if(returnPointer == NULL){return NULL;}
    return returnPointer;
}

void coalesce (void* block){
    int isWild = 0;

    //first look at the previous block
    void* prevFoot = block-8;
    size_t sizeOfBlock = (((sf_block*)block)->header & ~0x1F);
    void* orgBlockFooter = block + sizeOfBlock-8;
    sf_block* next = block + sizeOfBlock;
    //gets rid of the allocated bit
    ((sf_block*)block)->header = sizeOfBlock;
    memcpy(orgBlockFooter, &sizeOfBlock, 8);


    if((*(sf_footer*)prevFoot&THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED){//the previous is not allocated
        size_t prevSize = (*(sf_footer*)prevFoot & ~0x1F);
        sf_block* prev = block - prevSize;
        if(prev != prol){//check if it is the prologue
            //splice the list at that point of the previous
            sf_block* prevBlkPrev = prev->body.links.prev;
            sf_block* prevBlkNext = prev->body.links.next;
            prevBlkPrev->body.links.next = prevBlkNext;
            prevBlkNext->body.links.prev = prevBlkPrev;
            
            size_t newSize = prevSize + sizeOfBlock;
            block = prev;
            ((sf_block*)block)->header = newSize;
            memcpy(orgBlockFooter, &newSize, 8);
            //now block points to the new coallesced block woth prev with wrong next and prev pointers
            //adding it to a list will be done after cheching the one after this
        }
    }
    
    size_t nextSize = next->header & ~0x1F;
    sf_footer* nextFoot = ((void*)next) + nextSize - 8;
    if(((next->header & THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED) && next != epil){//check if the next is not allocated and if it is not equal to epilogue
        // splice the list and check if it is a wild block
        sf_block* nextBlkPrev = next->body.links.prev;
        sf_block* nextBlkNext = next->body.links.next;
        if((&sf_free_list_heads[7]) == nextBlkNext){isWild = 1;}//wildblock check
        nextBlkPrev->body.links.next = nextBlkNext;
        nextBlkNext->body.links.prev = nextBlkPrev;

        sizeOfBlock = (((sf_block*)block)->header & ~0x1F);
        size_t newSize = sizeOfBlock + nextSize;
        ((sf_block*)block)->header = newSize;
        memset(nextFoot, 0, 8);
        memcpy(nextFoot, &newSize, 8);
        //now all blocks that could have been coallesced are combined
    }
    if(isWild !=0){//is a wild block and should be in 7
        sf_free_list_heads[7].body.links.next = (sf_block*)block;
        sf_free_list_heads[7].body.links.prev = (sf_block*)block;
        ((sf_block*)block)->body.links.next = &sf_free_list_heads[7];
        ((sf_block*)block)->body.links.prev = &sf_free_list_heads[7];
        return;
    }else{
        //need to input it into a list that is not 7
        sizeOfBlock = (((sf_block*)block)->header & ~0x1F);
        int index = getListIndex(sizeOfBlock);
        sf_block* origNext = sf_free_list_heads[index].body.links.next;
        sf_free_list_heads[index].body.links.next = (sf_block*)block;
        ((sf_block*)block)->body.links.prev = &sf_free_list_heads[index];
        ((sf_block*)block)->body.links.next = origNext;
        origNext->body.links.prev = ((sf_block*)block);
    }
    
    return;
}

void sf_free(void *pp) {
    if((pp == NULL) || ((uintptr_t)pp%32)!=0){//checks if pointer is null and if the pointer is 32 byte alligned
        abort();
    }
    sf_block* blockPtr = pp-8;//gets the block pointer
    int sizeOfBlock = (blockPtr->header&~0x1F);//gets the size
    sf_footer* foot = (void*)blockPtr + sizeOfBlock;
    if((sizeOfBlock%32 != 0) || (sizeOfBlock<32) || ((blockPtr->header&THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED)){
        abort();
    }//allocation and sizes
    if(((uintptr_t)blockPtr<=(uintptr_t)prol) || ((uintptr_t)foot)>=(uintptr_t)epil){
        abort();
    }
    coalesce(blockPtr);
    return;
}
/*
 * Resizes the memory pointed to by ptr to size bytes.
 *
 * @param ptr Address of the memory region to resize.
 * @param size The minimum size to resize the memory to.
 *
 * @return If successful, the pointer to a valid region of memory is
 * returned, else NULL is returned and sf_errno is set appropriately.
 *
 *   If sf_realloc is called with an invalid pointer sf_errno should be set to EINVAL.
 *   If there is no memory available sf_realloc should set sf_errno to ENOMEM.
 *
 * If sf_realloc is called with a valid pointer and a size of 0 it should free
 * the allocated block and return NULL without setting sf_errno.
 */
void *sf_realloc(void *pp, size_t rsize) {
    if((pp == NULL) || ((uintptr_t)pp%32)!=0){//checks if pointer is null and if the pointer is 32 byte alligned
        sf_errno = EINVAL;
        return NULL;
    }
    sf_block* blockPtr = pp-8;//gets the block pointer
    int sizeOfBlock = (blockPtr->header&~0x1F);//gets the size
    sf_footer* foot = (void*)blockPtr + sizeOfBlock-8;
    if((sizeOfBlock%32 != 0) || (sizeOfBlock<32) || ((blockPtr->header&THIS_BLOCK_ALLOCATED) != THIS_BLOCK_ALLOCATED)){
        sf_errno = EINVAL;
        return NULL;
    }//allocation and sizes
    if(((uintptr_t)blockPtr<=(uintptr_t)prol) || ((uintptr_t)foot)>=(uintptr_t)epil){
        sf_errno = EINVAL;
        return NULL;
    }

    int pSize = (uintptr_t)foot- (uintptr_t)blockPtr-8;//the size of the payload
    if(pSize == rsize){ return pp;}//if they are the same then just return it

    if(pSize < rsize){//the size of the payload is smaller than the requested size
        void* x = sf_malloc(rsize);
        if(x == NULL){
            return NULL;
        }
        memcpy(x, pp, pSize);
        sf_free(pp);
        return x;
    }else if(pSize>rsize){//TODO finish this
        int sizeNeeded = rsize + 16;
        //use this to determine the necessary size of the block
        while(sizeNeeded % 32 != 0){
            sizeNeeded++;
        }
        if ((sizeOfBlock-sizeNeeded) %32 !=0 || (sizeOfBlock-sizeNeeded) ==0){ //with splinter so no splitting
            return pp;
        }else{//no splinter so we can split
            // sf_header blockHeader = blockPtr->header;//original header value
            //create splice the new block to return
            sf_header newBlockHeader = sizeNeeded | THIS_BLOCK_ALLOCATED;
            blockPtr->header = newBlockHeader; //set the new header
            sf_footer *returnBlockFooter = (void*)blockPtr+sizeNeeded-8;
            memcpy(returnBlockFooter, &newBlockHeader, 8);

            //create the new freed block
            sf_block* newFreedBlock = (void*)returnBlockFooter+8;
            sf_header newFreedBlockHeader = sizeOfBlock-sizeNeeded;
            newFreedBlock->header = newFreedBlockHeader;
            sf_footer *origblockFooter = (void*)blockPtr + sizeOfBlock-8;//original footer pointer
            memcpy(origblockFooter, &newFreedBlockHeader, 8);
            coalesce(newFreedBlock);

            return blockPtr->body.payload;
        }

    }

    // printf("%d\n", pSize);
    return NULL;
}
//0 if it is not a power of 2
//1 if it is

int power2(size_t check){
    while(check != 1){
        check /= 2;
        if(check % 2 != 0){
            if (check ==1){
                return 1;
            }else{
                return 0;
            }
        }
    }
    return 1;
}

void* splitEnd(size_t size, size_t align, void* pp){
    sf_block* ppBlock = pp-8;
    size_t sizeOfOrig = ppBlock->header & ~0x1F;
    size_t sizeOfNew = sizeOfOrig-size;
    sf_footer* footerOfNew= (void*)ppBlock + sizeOfOrig - 8;
    if(sizeOfNew == 0 || sizeOfNew%32 !=0){return pp;}//if the size of the freed block is 0 or if the size of the freed block is not a multiple of 32
    //now break the blocks into two and split them;
    //return block
    sf_header newPPHeader = size | THIS_BLOCK_ALLOCATED;
    ppBlock->header = newPPHeader;
    sf_footer* ppFooter = (void*)ppBlock + size - 8;
    memcpy(ppFooter, &newPPHeader, 8);

    //freed Block
    sf_block* newFreedBlock = (void*)ppFooter + 8;
    sf_header newFreedHead = sizeOfNew;
    newFreedBlock->header = newFreedHead;
    memcpy(footerOfNew, &newFreedHead, 8);
    coalesce(newFreedBlock);
    return ppBlock->body.payload;
}

/*
 * Allocates a block of memory with a specified alignment.
 *
 * @param align The alignment required of the returned pointer.
 * @param size The number of bytes requested to be allocated.
 *
 * @return If align is not a power of two or is less than the minimum block size,
 * then NULL is returned and sf_errno is set to EINVAL.
 * If size is 0, then NULL is returned without setting sf_errno.
 * Otherwise, if the allocation is successful a pointer to a valid region of memory
 * of the requested size and with the requested alignment is returned.
 * If the allocation is not successful, then NULL is returned and sf_errno is set
 * to ENOMEM.
 */
void *sf_memalign(size_t size, size_t align) {
    if(size == 0){return NULL;}
    if(align < 32 || power2(align)==0){
        sf_errno = EINVAL;
        return NULL;
    }
    //error handling done
    size_t totalSize = size + align + 32;
    void* pp = sf_malloc(totalSize);
    if(pp == NULL){return NULL;}

    int sizeNeeded = size + 16;
    //use this to determine the necessary size of the block
    while(sizeNeeded % 32 != 0){
        sizeNeeded++;
    }
    //determine what to do with  the current pointer
    if((uintptr_t)pp % align == 0){
        //need to check and cut off end
        return splitEnd(sizeNeeded, align, pp);
    }else{
        //need to incrememnt the pointer untill it is aligned
        sf_block* originalBlock = (void*)pp - 8;
        size_t originalSize= originalBlock->header & ~0x1F;
        sf_footer* originalFooter = (void*)originalBlock + originalSize-8;
        size_t count= 0;
        while((uintptr_t)pp % align != 0){
            pp++;
            count++;
        }
        //pp now points the a correctly aligned address now change the header
        sf_block* newPPBlock = (void*)pp -8;
        sf_header newPPBlockHead = (originalSize-count) | THIS_BLOCK_ALLOCATED;
        newPPBlock->header = newPPBlockHead;
        memcpy(originalFooter, &newPPBlockHead, 8);


        //free old block and coalesce it
        size_t newLength = (uintptr_t)newPPBlock - (uintptr_t)originalBlock;
        originalBlock->header = newLength;
        sf_footer* oldFooter = (void*)originalBlock + newLength-8;
        memcpy(oldFooter, &newLength, 8);//save new footer to previous "free block"
        coalesce(originalBlock);
        return splitEnd(sizeNeeded, align, pp);

    }

    return NULL;
}
