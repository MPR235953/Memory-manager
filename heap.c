#include <string.h>
#include <stdio.h>
#include "heap.h"
#include "custom_unistd.h"

//[KKKKAFfffffffffffffffffffffB]

int heap_show(int flag){
    if(flag == 1)
        printf("\n###############################################    AFTER MALLOC   ######################################################\n");
    else if(flag == 2)
        printf("\n###############################################    AFTER REALLOC   ######################################################\n");
    else if(flag == 3)
        printf("\n###############################################    AFTER FREE   ######################################################\n");
    struct block_t *temp = heap->head->next;
    int i = 1;
    int exit = 0;
    printf("\nBlok head: %d B\n", heap->head->block_size);
    while(temp != heap->tail){
        if(temp->block_size < 0)
            printf("Blok wolny nr %d: %d B\n", i, temp->block_size);
        else if(temp->block_size > 0)
            printf("Blok zajęty nr %d: %d B\n", i, temp->block_size);
        else {
            printf("#################################################### ALERT dla bloku nr %d: %d B\n", i, temp->block_size);
            exit = 1;
        }
        temp = temp->next;
        i++;
    }
    printf("Blok tail: %d B\n", heap->tail->block_size);
    if(exit) return 1;
    return 0;
}

int heap_setup(void){
    heap = custom_sbrk(HEAP_SIZE);
    if(heap == (void*)-1) return -1;
    heap->heap_size = HEAP_SIZE;
    heap->head = (struct block_t *)((uint8_t *)heap + SIZEOF_HEAP);
    heap->tail = (struct block_t *)((uint8_t *)heap + heap->heap_size - SIZEOF_BLOCK);
    struct block_t* free_block = (struct block_t *)((uint8_t *)heap->head + SIZEOF_BLOCK);

    heap->head->block_size = 0;
    free_block->block_size = -(int)((uint8_t *)heap->tail - (uint8_t *)free_block - SIZEOF_BLOCK);
    heap->tail->block_size = 0;

    heap->head->prev = NULL;
    heap->head->next = free_block;
    free_block->prev = heap->head;
    free_block->next = heap->tail;
    heap->tail->prev = free_block;
    heap->tail->next = NULL;

    free_block->magic1 = MAGIC1;
    heap->head->magic1 = MAGIC1;
    heap->tail->magic1 = MAGIC1;
    free_block->magic2 = MAGIC2;
    heap->head->magic2 = MAGIC2;
    heap->tail->magic2 = MAGIC2;

    free_block->check_sum = COUNT_CHECKSUM(free_block);
    heap->head->check_sum = COUNT_CHECKSUM(heap->head);
    heap->tail->check_sum = COUNT_CHECKSUM(heap->tail);
    return 0;
}

void heap_clean(void){
    if(!heap) return;
    heap = custom_sbrk(-heap->heap_size);
    heap = NULL;
}


int heap_increase(size_t size){
    if(heap_validate() || size == 0) return 0;
    struct block_t *temp_ptr = heap->tail->prev;
    struct block_t *tail_copy = heap->tail;
    heap->tail = custom_sbrk(size);
    if(heap->tail == (void*)-1){
        heap->tail = tail_copy;
        return 0;
    }
    heap->tail = (struct block_t *)((uint8_t *)tail_copy + size - SIZEOF_BLOCK);
    heap->heap_size += size;
    heap->tail->next = NULL;
    heap->tail->block_size = 0;
    heap->tail->magic1 = MAGIC1;
    heap->tail->magic2 = MAGIC2;
    if(temp_ptr->block_size > 0){   //przypadek gdy rozszerzamy sterte i na koncu nie ma bloku pustego, wiec trzeba go zrobic
        struct block_t *free_block = (struct block_t *)((uint8_t *)temp_ptr + SIZEOF_BLOCK + ALIGN(temp_ptr->block_size) + 2 * FENCE_SIZE);
        free_block->next = heap->tail;
        heap->tail->prev = free_block;
        free_block->prev = temp_ptr;
        temp_ptr->next = free_block;
        free_block->block_size = -(int)((uint8_t *)heap->tail - (uint8_t *)free_block - SIZEOF_BLOCK);
        free_block->magic1 = MAGIC1;
        free_block->magic2 = MAGIC2;
        free_block->check_sum = COUNT_CHECKSUM(free_block);
        temp_ptr->check_sum = COUNT_CHECKSUM(temp_ptr);
    }
    else {                          //przypadek gdy na koncu jest blok pusty, wiec trzeba go zmodyfikowac
        heap->tail->prev = temp_ptr;
        temp_ptr->next = heap->tail;
        temp_ptr->block_size = -(int)((uint8_t *)temp_ptr->next - (uint8_t *)temp_ptr - SIZEOF_BLOCK);
        temp_ptr->check_sum = COUNT_CHECKSUM(heap->tail->prev);
    }
    heap->tail->check_sum = COUNT_CHECKSUM(heap->tail);
    return 1;
}

void* heap_malloc(size_t size){
    if(size == 0 || heap_validate()) return NULL;
    struct block_t *temp = heap->head->next;
    struct block_t *new_free_block;
    size_t aligned_size = ALIGN(size);
    while(1){
        if(temp == heap->tail){ //przerwanie jesli dojdziemy do granicy sterty i jej powiekszenie sie nie powiedzie
            struct block_t *prev = temp->prev;
            if(!heap_increase(aligned_size + SIZEOF_BLOCK + 2 * FENCE_SIZE))return NULL;
            else temp = prev;
        }
        if(-temp->block_size - 2 * (int)FENCE_SIZE >= (int)aligned_size){    //zmiana bloku wolnego w zajety
            temp->block_size = size;
            memset((uint8_t *)temp + SIZEOF_BLOCK, '#', FENCE_SIZE);
            memset((uint8_t *)temp + SIZEOF_BLOCK + temp->block_size + FENCE_SIZE, '#', FENCE_SIZE);
            if((uint8_t *)temp->next - (uint8_t *)temp - SIZEOF_BLOCK - aligned_size - 2 * FENCE_SIZE > SIZEOF_BLOCK) {    //jesli starczy pamieci na naglowek nowego bloku wolnego
                new_free_block = (struct block_t *) ((uint8_t *) temp + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE);
                temp->next->prev = new_free_block;
                new_free_block->next = temp->next;
                temp->next = new_free_block;
                new_free_block->prev = temp;
                new_free_block->block_size = -(int)((uint8_t *) new_free_block->next - (uint8_t *) new_free_block - SIZEOF_BLOCK);
                new_free_block->magic1 = MAGIC1;
                new_free_block->magic2 = MAGIC2;
                new_free_block->check_sum = COUNT_CHECKSUM(new_free_block);
                new_free_block->next->check_sum = COUNT_CHECKSUM(new_free_block->next);
            }
            temp->check_sum = COUNT_CHECKSUM(temp);
            return (intptr_t *)((uint8_t *)temp + SIZEOF_BLOCK + FENCE_SIZE);
        }
        temp = temp->next;
    }
}

void* heap_calloc(size_t number, size_t size){
    if(number == 0 || size == 0 || heap_validate()) return NULL;
    intptr_t *ptr = heap_malloc(number * size);
    if(!ptr) return  NULL;
    struct block_t *control_block = (struct block_t *)((uint8_t *)ptr - FENCE_SIZE - SIZEOF_BLOCK);
    memset((uint8_t *)control_block + FENCE_SIZE + SIZEOF_BLOCK, 0, control_block->block_size);
    return (intptr_t *)ptr;
}

void* heap_realloc(void* memblock, size_t count){
    if(heap_validate()) return NULL;
    if(count == 0) {
        heap_free(memblock);
        return NULL;
    }

    if(!memblock) return heap_malloc(count);
    if(get_pointer_type(memblock) != pointer_valid) return NULL;
    struct block_t *control_block = (struct block_t *)((uint8_t *)memblock - FENCE_SIZE - SIZEOF_BLOCK);

    if(control_block->block_size == (int)count) return (intptr_t *)memblock;
    else if(control_block->block_size > (int)count){        //zmniejszanie rozmiaru zajętego bloku
        struct block_t *free_block = control_block->next;
        int diff = control_block->block_size - (int)count;
        int aligned_diff = ALIGN(control_block->block_size) - ALIGN(count);
        control_block->block_size -= diff;
        size_t aligned_size = ALIGN(control_block->block_size);
        memset((uint8_t *)control_block + SIZEOF_BLOCK + control_block->block_size + FENCE_SIZE, '#', FENCE_SIZE);

        if(free_block->block_size < 0){     //jesli blok jest wolny to przenoszony jest jego naglówek do tylu
            memcpy((uint8_t *)control_block + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE,(uint8_t *)free_block, SIZEOF_BLOCK);
            free_block = (struct block_t *)((uint8_t *)control_block + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE);
            free_block->next->prev = free_block;
            free_block->prev->next = free_block;
            free_block->block_size = -(int)((uint8_t *)free_block->next - (uint8_t *)free_block - SIZEOF_BLOCK);
            free_block->check_sum = COUNT_CHECKSUM(free_block);
            free_block->next->check_sum = COUNT_CHECKSUM(free_block->next);
        }
        else if(aligned_diff > (int)SIZEOF_BLOCK){     //jezeli blok zmniejszany mial sasaida (lub byl ostatnim blokiem) i ten blok byl zajęty a rozmiar o jaki był zmniejszany blok docelowy pozwala na utworzenie bloku wolnego to go tworzy
            struct block_t *new_free_block = (struct block_t *) ((uint8_t *) control_block + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE);
            new_free_block->next = control_block->next;
            control_block->next->prev = new_free_block;
            control_block->next = new_free_block;
            new_free_block->prev = control_block;
            new_free_block->block_size = -(int)((uint8_t *)new_free_block->next - (uint8_t *)new_free_block - SIZEOF_BLOCK);
            new_free_block->magic1 = MAGIC1;
            new_free_block->magic2 = MAGIC2;
            new_free_block->check_sum = COUNT_CHECKSUM(new_free_block);
            new_free_block->next->check_sum = COUNT_CHECKSUM(new_free_block->next);
        }
        control_block->check_sum = COUNT_CHECKSUM(control_block);
        return (intptr_t *)memblock;
    }
    else{   //zwiększanie rozmiaru zajetego bloku
        struct block_t *free_block = control_block->next;
        int to_increase = (int)count - control_block->block_size;
        int aligned_new_size = ALIGN(count);
        int aligned_old_size = ALIGN(control_block->block_size);
        int to_decrease = to_increase + (aligned_new_size - (int)count) - (aligned_old_size - control_block->block_size);
        if(-free_block->block_size > to_decrease){     //jesli po rozszerzeniu bloku zajetego, blok wolny moze nadal istniec
            control_block->block_size = count;
            free_block->block_size += to_decrease;
            memcpy((uint8_t *)control_block + SIZEOF_BLOCK + aligned_new_size + 2 * FENCE_SIZE, free_block, SIZEOF_BLOCK);
            memset((uint8_t *)control_block + SIZEOF_BLOCK + control_block->block_size + FENCE_SIZE, '#', FENCE_SIZE);
            free_block = (struct block_t *)((uint8_t *)control_block + SIZEOF_BLOCK + aligned_new_size + 2 * FENCE_SIZE);
            free_block->next->prev = free_block;
            free_block->prev->next = free_block;
            free_block->check_sum = COUNT_CHECKSUM(free_block);
            free_block->next->check_sum = COUNT_CHECKSUM(free_block->next);
        }
        else if(free_block->block_size < 0 && -free_block->block_size + (int)SIZEOF_BLOCK >= to_decrease){     //jesli po rozszerzeniu bloku zajetego, blok wolny nie moze juz istniec
            control_block->block_size = count;
            control_block->next = control_block->next->next;
            control_block->next->prev = control_block;
            memset((uint8_t *)control_block + SIZEOF_BLOCK + control_block->block_size + FENCE_SIZE, '#', FENCE_SIZE);
            control_block->next->check_sum = COUNT_CHECKSUM(control_block->next);
        }
        else{
            struct block_t *temp = control_block->next;
            int temp_size = temp->block_size;
            int aligned_count = ALIGN(count);
            while(1){
                if(!temp->block_size){                  //w przypadku braku miejsca na stercie
                    if(!heap_increase(aligned_count + 2 * FENCE_SIZE)) return NULL;
                    return heap_realloc(memblock, count);
                }

                if(-temp->block_size >= aligned_count + 2 * (int)FENCE_SIZE){       //jezeli przenoszony blok zmiesci sie w bloku wolnym
                    memcpy((uint8_t *)temp + 2 * PTR_SIZE + 4, (uint8_t *)control_block + 2 * PTR_SIZE + 4, PTR_SIZE + 4 + control_block->block_size + FENCE_SIZE);     //przeniesienie bloku zajetego w miejsce bloku wolnego
                    temp->block_size = (int)count;
                    memset((uint8_t *)temp + SIZEOF_BLOCK + temp->block_size + FENCE_SIZE, '#', FENCE_SIZE);
                    temp->check_sum = COUNT_CHECKSUM(temp);
                    temp_size += aligned_count + 2 * FENCE_SIZE;
                    heap_free((uint8_t *)control_block + SIZEOF_BLOCK + FENCE_SIZE);
                    if(-temp_size > (int)SIZEOF_BLOCK){     //jesli po przeniesieniu bloku zostanie jeszcze miejsce na nowy blok wolny
                        struct block_t *new_free_block = (struct block_t *)((uint8_t *)temp + SIZEOF_BLOCK + aligned_count + 2 * FENCE_SIZE);
                        new_free_block->next = temp->next;
                        new_free_block->prev = temp;
                        temp->next->prev = new_free_block;
                        temp->next = new_free_block;
                        new_free_block->block_size = temp_size + (int)SIZEOF_BLOCK;
                        new_free_block->magic1 = MAGIC1;
                        new_free_block->magic2 = MAGIC2;
                        new_free_block->check_sum = COUNT_CHECKSUM(new_free_block);
                        new_free_block->next->check_sum = COUNT_CHECKSUM(new_free_block->next);
                    }
                    temp->check_sum = COUNT_CHECKSUM(temp);
                    return (intptr_t *)((uint8_t *)temp + SIZEOF_BLOCK + FENCE_SIZE);
                }
                temp = temp->next;
                temp_size = temp->block_size;
            }
        }
        control_block->check_sum = COUNT_CHECKSUM(control_block);
        return (intptr_t *)memblock;
    }
}

void  heap_free(void* memblock){
    if(!memblock || heap_validate() || !good_address((uint8_t *)memblock - FENCE_SIZE - SIZEOF_BLOCK)) return;
    struct block_t *control_block = (struct block_t *)((uint8_t *)memblock - FENCE_SIZE - SIZEOF_BLOCK);

    control_block->block_size *= (-1);          //przejscie do struktury kontrolnej addressu

    //ustawienie nowego rozmiaru dla sztuktury kontrolnej address w przypadku wystąpienia wolnego miejsca pomiedzy blokami
    control_block->block_size = -(int)((uint8_t *)control_block->next - (uint8_t *)control_block - SIZEOF_BLOCK);

    if(control_block->next->block_size < 0){    //przypadek istnienia wolnego bloku za blokiem address
        control_block->block_size += control_block->next->block_size -(int)SIZEOF_BLOCK;
        control_block->next = control_block->next->next;
        control_block->next->prev = control_block;
        control_block->next->check_sum = COUNT_CHECKSUM(control_block->next);
    }

    if(control_block->prev->block_size < 0){    //przypadek istnienia wolnego bloku przed blokiem address
        int temp_size = control_block->block_size;
        struct block_t *temp_node = control_block->next;
        control_block = control_block->prev;
        control_block->block_size += temp_size -(int)SIZEOF_BLOCK;
        control_block->next = temp_node;
        temp_node->prev = control_block;
        control_block->next->check_sum = COUNT_CHECKSUM(control_block->next);
    }
    control_block->check_sum = COUNT_CHECKSUM(control_block);
}

int heap_validate(void){
    if(!heap || heap == (void*)-1) return 2;
    struct block_t *temp = heap->head->next;
    while(temp != heap->tail){
        if(!temp->next || !temp->prev)
            return 3;
        if(temp->magic1 != MAGIC1 || temp->magic2 != MAGIC2)
            return 3;
        int check_sum = COUNT_CHECKSUM(temp);
        if(check_sum != temp->check_sum)
            return 3;
        if(temp->block_size > 0 && (memcmp((char *)((uint8_t *)temp + SIZEOF_BLOCK), "########", FENCE_SIZE) != 0 ||
                                    memcmp((char *)((uint8_t *)temp + SIZEOF_BLOCK + FENCE_SIZE + temp->block_size), "########", FENCE_SIZE) != 0)) return 1;
        temp = temp->next;
    }
    return 0;
}

int good_address(void *address){
    if(!address) return 0;
    struct block_t *temp = heap->head->next;
    while(temp != heap->tail) {
        if (temp == (struct block_t *)address) return 1;
        temp = temp->next;
    }
    return 0;
}

size_t heap_get_largest_used_block_size(void){
    if(heap == NULL || heap_validate()) return 0;
    struct block_t *temp = heap->head->next;
    int max = 0;
    while(temp != heap->tail){
        if(temp->block_size > max) max = temp->block_size;
        temp = temp->next;
    }
    return max;
}

enum pointer_type_t get_pointer_type(const void* const pointer){
    if(!pointer) return pointer_null;
    if(heap_validate()) return pointer_heap_corrupted;

    struct block_t *temp = heap->head->next;
    while(temp != heap->tail){
        if((uint8_t *)temp - (uint8_t *)pointer <= 0){
            if((uint8_t *)temp->next - (uint8_t *)pointer > 0)
                break;
        }
        temp = temp->next;
    }

    int pointres_diff = (int)((uint8_t *)pointer - (uint8_t *)temp);

    if(temp->block_size > 0) {
        if (pointres_diff >= 0 && pointres_diff < (int)SIZEOF_BLOCK) return pointer_control_block;
        else if ((pointres_diff >= (int)SIZEOF_BLOCK && pointres_diff < (int)(SIZEOF_BLOCK + FENCE_SIZE)) ||
                 (pointres_diff >= (int)(SIZEOF_BLOCK + FENCE_SIZE + temp->block_size) && pointres_diff < (int)(SIZEOF_BLOCK + FENCE_SIZE + temp->block_size + FENCE_SIZE)))
            return pointer_inside_fences;
        else if (pointres_diff == (int)(SIZEOF_BLOCK + FENCE_SIZE)) return pointer_valid;
        else if (pointres_diff > (int)(SIZEOF_BLOCK + FENCE_SIZE) && pointres_diff < (int)(SIZEOF_BLOCK + FENCE_SIZE + temp->block_size)) return pointer_inside_data_block;
        else return pointer_unallocated;
    }
    else{
        if (pointres_diff >= 0 && pointres_diff < (int)SIZEOF_BLOCK) return pointer_control_block;
        else return pointer_unallocated;
    }
}


void* heap_malloc_aligned(size_t size){
    if(size == 0 || heap_validate()) return NULL;
    struct block_t * temp = heap->head->next;
    struct block_t *new_free_block = NULL;
    size_t aligned_size = ALIGN(size);

    while(1){
        void *aligned_data_address = ALIGN_TO_PAGE(temp);
        struct block_t *aligned_address = (struct block_t *)((uint8_t *)aligned_data_address - SIZEOF_BLOCK - FENCE_SIZE);

        if(temp == heap->tail){
            struct block_t *prev = temp->prev;
            int to_increase = (int)aligned_size + FENCE_SIZE;
            if((uint8_t *)aligned_address - (uint8_t *)temp < (int)SIZEOF_BLOCK + (int)FENCE_SIZE){     //jesli naglówek bloku wyrównanego bedzie przesłaniał blok koncowy
                aligned_data_address = (void *)((uint8_t *)aligned_data_address + PAGE_SIZE);
                aligned_address = (struct block_t *)((uint8_t *)aligned_address + PAGE_SIZE);
                to_increase += PAGE_SIZE;
            }
            if(to_increase < PAGE_SIZE) to_increase += PAGE_SIZE;
            if(!heap_increase(to_increase + PAGE_SIZE)) return NULL;
            temp = prev;
            if(temp->block_size > 0)
                temp = temp->next;
            aligned_data_address = (void *)((uint8_t *)aligned_data_address - PAGE_SIZE);
            aligned_address = (struct block_t *)((uint8_t *)aligned_address - PAGE_SIZE);
        }

        //czy nie zostaną zamazane dane wczesniejszego bloku i czy dane zmieszcza sie w bloku wolnym
        if(temp->block_size < 0 && (uint8_t *)aligned_address - (uint8_t *)temp >= 0 && (uint8_t *)temp->next - (uint8_t *)aligned_data_address >= (int)aligned_size + (int)FENCE_SIZE){
            if((uint8_t *)aligned_address - (uint8_t *)temp <= (int)SIZEOF_BLOCK){ //jesli adres wyrównany przesłoni strukture bloku wolnego
                struct block_t *next = temp->next, *prev = temp->prev;
                aligned_address->next = next;
                aligned_address->prev = prev;
                aligned_address->next->prev = aligned_address;
                aligned_address->prev->next = aligned_address;
            }
            else{       //jesli starczy miejsca aby blok temp mogl dalej istniec
                temp->block_size = -(int)((uint8_t *)aligned_address - (uint8_t *)temp - SIZEOF_BLOCK);
                aligned_address->next = temp->next;
                aligned_address->prev = temp;
                aligned_address->next->prev = aligned_address;
                temp->next = aligned_address;
            }

            //jesli pomiedzy wyrownanym adresem a kolejnym blokiem starczy miejsca na blok wolny
            if((uint8_t *)aligned_address->next - (uint8_t *)aligned_data_address - aligned_size - FENCE_SIZE > SIZEOF_BLOCK){
                new_free_block = (struct block_t *)((uint8_t *)aligned_data_address + aligned_size + FENCE_SIZE);
                new_free_block->next = aligned_address->next;
                new_free_block->prev = aligned_address;
                new_free_block->next->prev = new_free_block;
                new_free_block->prev->next = new_free_block;
                new_free_block->block_size = -(int)((uint8_t *)new_free_block->next - (uint8_t *)new_free_block - SIZEOF_BLOCK);
                new_free_block->magic1 = MAGIC1;
                new_free_block->magic2 = MAGIC2;
                new_free_block->next->check_sum = COUNT_CHECKSUM(new_free_block->next);
            }

            aligned_address->block_size = size;
            aligned_address->magic1 = MAGIC1;
            aligned_address->magic2 = MAGIC2;
            memset((uint8_t *)aligned_address + SIZEOF_BLOCK, '#', FENCE_SIZE);
            memset((uint8_t *)aligned_address + SIZEOF_BLOCK + FENCE_SIZE + size, '#', FENCE_SIZE);
            aligned_address->check_sum = COUNT_CHECKSUM(aligned_address);
            aligned_address->next->check_sum = COUNT_CHECKSUM(aligned_address->next);
            aligned_address->prev->check_sum = COUNT_CHECKSUM(aligned_address->prev);
            return (intptr_t *)aligned_data_address;
        }
        temp = temp->next;
    }
}

void* heap_calloc_aligned(size_t number, size_t size){
    if(number == 0 || size == 0 || heap_validate()) return NULL;
    intptr_t *ptr = heap_malloc_aligned(number * size);
    if(!ptr) return  NULL;
    struct block_t *control_block = (struct block_t *)((uint8_t *)ptr - FENCE_SIZE - SIZEOF_BLOCK);
    memset((uint8_t *)control_block + FENCE_SIZE + SIZEOF_BLOCK, 0, control_block->block_size);
    return (intptr_t *)ptr;
}

void* heap_realloc_aligned(void* memblock, size_t count){
    if(heap_validate()) return NULL;
    if(count == 0) {
        heap_free(memblock);
        return NULL;
    }

    if(!memblock) return heap_malloc_aligned(count);
    if(get_pointer_type(memblock) != pointer_valid) return NULL;
    struct block_t *control_block = (struct block_t *)((uint8_t *)memblock - FENCE_SIZE - SIZEOF_BLOCK);

    if(control_block->block_size == (int)count) return (intptr_t *)memblock;
    else if(control_block->block_size > (int)count){        //zmniejszanie rozmiaru zajętego bloku
        struct block_t *free_block = control_block->next;
        int diff = control_block->block_size - (int)count;
        int aligned_diff = ALIGN(control_block->block_size) - ALIGN(count);
        control_block->block_size -= diff;
        size_t aligned_size = ALIGN(control_block->block_size);
        memset((uint8_t *)control_block + SIZEOF_BLOCK + control_block->block_size + FENCE_SIZE, '#', FENCE_SIZE);

        if(free_block->block_size < 0){     //jesli blok jest wolny to przenoszony jest jego naglówek do tylu
            memcpy((uint8_t *)control_block + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE,(uint8_t *)free_block, SIZEOF_BLOCK);
            free_block = (struct block_t *)((uint8_t *)control_block + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE);
            free_block->next->prev = free_block;
            free_block->prev->next = free_block;
            free_block->block_size = -(int)((uint8_t *)free_block->next - (uint8_t *)free_block - SIZEOF_BLOCK);
            free_block->check_sum = COUNT_CHECKSUM(free_block);
            free_block->next->check_sum = COUNT_CHECKSUM(free_block->next);
        }
        else if(aligned_diff > (int)SIZEOF_BLOCK){     //jezeli blok zmniejszany mial sasaida (lub byl ostatnim blokiem) i ten blok byl zajęty a rozmiar o jaki był zmniejszany blok docelowy pozwala na utworzenie bloku wolnego to go tworzy
            struct block_t *new_free_block = (struct block_t *) ((uint8_t *) control_block + SIZEOF_BLOCK + aligned_size + 2 * FENCE_SIZE);
            new_free_block->next = control_block->next;
            control_block->next->prev = new_free_block;
            control_block->next = new_free_block;
            new_free_block->prev = control_block;
            new_free_block->block_size = -(int)((uint8_t *)new_free_block->next - (uint8_t *)new_free_block - SIZEOF_BLOCK);
            new_free_block->magic1 = MAGIC1;
            new_free_block->magic2 = MAGIC2;
            new_free_block->check_sum = COUNT_CHECKSUM(new_free_block);
            new_free_block->next->check_sum = COUNT_CHECKSUM(new_free_block->next);
        }
        control_block->check_sum = COUNT_CHECKSUM(control_block);
        return (intptr_t *)memblock;
    }
    else{   //zwiększanie rozmiaru zajetego bloku
        struct block_t *free_block = control_block->next;
        int to_increase = (int)count - control_block->block_size;
        int aligned_new_size = ALIGN(count);
        int aligned_old_size = ALIGN(control_block->block_size);
        int to_decrease = to_increase + (aligned_new_size - (int)count) - (aligned_old_size - control_block->block_size);
        if(-free_block->block_size > to_decrease){     //jesli po rozszerzeniu bloku zajetego, blok wolny moze nadal istniec
            control_block->block_size = count;
            free_block->block_size += to_decrease;
            memcpy((uint8_t *)control_block + SIZEOF_BLOCK + aligned_new_size + 2 * FENCE_SIZE, free_block, SIZEOF_BLOCK);
            memset((uint8_t *)control_block + SIZEOF_BLOCK + control_block->block_size + FENCE_SIZE, '#', FENCE_SIZE);
            free_block = (struct block_t *)((uint8_t *)control_block + SIZEOF_BLOCK + aligned_new_size + 2 * FENCE_SIZE);
            free_block->next->prev = free_block;
            free_block->prev->next = free_block;
            free_block->check_sum = COUNT_CHECKSUM(free_block);
            free_block->next->check_sum = COUNT_CHECKSUM(free_block->next);
        }
        else if(free_block->block_size < 0 && -free_block->block_size + (int)SIZEOF_BLOCK >= to_decrease){     //jesli po rozszerzeniu bloku zajetego, blok wolny nie moze juz istniec
            control_block->block_size = count;
            control_block->next = control_block->next->next;
            control_block->next->prev = control_block;
            memset((uint8_t *)control_block + SIZEOF_BLOCK + control_block->block_size + FENCE_SIZE, '#', FENCE_SIZE);
            control_block->next->check_sum = COUNT_CHECKSUM(control_block->next);
        }
        else{
            struct block_t *temp = control_block->next;
            int aligned_count = ALIGN(count);
            while(1){
                void *aligned_data_address = ALIGN_TO_PAGE(temp);
                struct block_t *aligned_address = (struct block_t *)((uint8_t *)aligned_data_address - SIZEOF_BLOCK - FENCE_SIZE);

                if(!temp->block_size){                  //w przypadku braku miejsca na stercie
                    struct block_t *prev = temp->prev;
                    to_increase = (int)aligned_count + FENCE_SIZE;
                    if((uint8_t *)aligned_address - (uint8_t *)temp < (int)SIZEOF_BLOCK + (int)FENCE_SIZE){     //jesli naglówek bloku wyrównanego bedzie przesłaniał blok koncowy
                        aligned_data_address = (void *)((uint8_t *)aligned_data_address + PAGE_SIZE);
                        aligned_address = (struct block_t *)((uint8_t *)aligned_address + PAGE_SIZE);
                        to_increase += PAGE_SIZE;
                    }
                    if(to_increase < PAGE_SIZE) to_increase += PAGE_SIZE;
                    if(!heap_increase(to_increase + PAGE_SIZE)) return NULL;
                    temp = prev;
                    if(temp->prev == control_block)
                        return heap_realloc_aligned(memblock, count);
                    if(temp->block_size > 0)
                        temp = temp->next;
                    aligned_data_address = (void *)((uint8_t *)aligned_data_address - PAGE_SIZE);
                    aligned_address = (struct block_t *)((uint8_t *)aligned_address - PAGE_SIZE);
                }

                if(temp->block_size < 0 && (uint8_t *)aligned_address - (uint8_t *)temp >= 0 && (uint8_t *)temp->next - (uint8_t *)aligned_data_address >= (int)aligned_count + (int)FENCE_SIZE){
                    if((uint8_t *)aligned_address - (uint8_t *)temp <= (int)SIZEOF_BLOCK){ //jesli adres wyrównany przesłoni strukture bloku wolnego
                        struct block_t *next = temp->next, *prev = temp->prev;
                        aligned_address->next = next;
                        aligned_address->prev = prev;
                        aligned_address->next->prev = aligned_address;
                        aligned_address->prev->next = aligned_address;
                    }
                    else{       //jesli starczy miejsca aby blok temp mogl dalej istniec
                        temp->block_size = -(int)((uint8_t *)aligned_address - (uint8_t *)temp - SIZEOF_BLOCK);
                        aligned_address->next = temp->next;
                        aligned_address->prev = temp;
                        aligned_address->next->prev = aligned_address;
                        temp->next = aligned_address;
                    }

                    //jesli pomiedzy wyrownanym adresem a kolejnym blokiem starczy miejsca na blok wolny
                    if((uint8_t *)aligned_address->next - (uint8_t *)aligned_data_address - aligned_count - FENCE_SIZE > (int)SIZEOF_BLOCK){
                        struct block_t *new_free_block = (struct block_t *)((uint8_t *)aligned_data_address + aligned_count + FENCE_SIZE);
                        new_free_block->next = aligned_address->next;
                        new_free_block->prev = aligned_address;
                        new_free_block->next->prev = new_free_block;
                        new_free_block->prev->next = new_free_block;
                        new_free_block->block_size = -(int)((uint8_t *)new_free_block->next - (uint8_t *)new_free_block - SIZEOF_BLOCK);
                        new_free_block->magic1 = MAGIC1;
                        new_free_block->magic2 = MAGIC2;
                        new_free_block->next->check_sum = COUNT_CHECKSUM(new_free_block->next);
                    }

                    memcpy((uint8_t *)aligned_data_address, (uint8_t *)memblock, control_block->block_size);
                    aligned_address->block_size = count;
                    aligned_address->magic1 = MAGIC1;
                    aligned_address->magic2 = MAGIC2;
                    memset((uint8_t *)aligned_address + SIZEOF_BLOCK, '#', FENCE_SIZE);
                    memset((uint8_t *)aligned_address + SIZEOF_BLOCK + FENCE_SIZE + count, '#', FENCE_SIZE);
                    aligned_address->check_sum = COUNT_CHECKSUM(aligned_address);
                    aligned_address->next->check_sum = COUNT_CHECKSUM(aligned_address->next);
                    aligned_address->prev->check_sum = COUNT_CHECKSUM(aligned_address->prev);
                    heap_free(memblock);
                    return (intptr_t *)aligned_data_address;
                }
                temp = temp->next;
            }
        }
        control_block->check_sum = COUNT_CHECKSUM(control_block);
        return (intptr_t *)memblock;
    }
}

//################################################################################################################################################################

void* heap_malloc_debug(size_t count, int fileline, const char* filename){
    if(count == 0 || !fileline || !filename) return NULL;
    return NULL;
}

void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename){
    if(number == 0 || size == 0 || !fileline || !filename) return NULL;
    return NULL;
}

void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename){
    if(!memblock || size == 0 || !fileline || !filename) return NULL;
    return NULL;
}


void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename){
    if(count == 0 || !fileline || !filename) return NULL;
    return NULL;
}

void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename){
    if(number == 0 || size == 0 || !fileline || !filename) return NULL;
    return NULL;
}

void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename){
    if(!memblock || size == 0 || !fileline || !filename) return NULL;
    return NULL;
}
