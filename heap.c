//
// Created by root on 12/10/22.
//
#include "heap.h"






int64_t checksum(void *ptr)
{
    if(ptr == NULL) return -1;

    int64_t  sum =0;
    struct heap_mem *header = (struct heap_mem *) ptr;


    if(header->prev == NULL) return sum;
    union pointer tmp;
    tmp.size = (int64_t ) header->prev;
    for(int i=0; i<8; i++)
    {
        sum += tmp.bytes[i];
    }

    if(header->next == NULL) return sum;
    tmp.size = (int64_t ) header->next;
    for(int i=0; i<8; i++)
    {
        sum += tmp.bytes[i];
    }

    tmp.size = (int64_t ) header->size;
    for(int i=0; i<8; i++)
    {
        sum += tmp.bytes[i];
    }
    return sum;
}
int heap_setup(void){

    void * tmp = custom_sbrk(PAGE_SIZE);
    if(*(int *)tmp == -1) return -1;
    // pierwsza zaslepka
    heapMem = (struct heap_mem *) tmp;
    heapMem->prev = NULL;
    heapMem->size = 0 ;

    //pierwszy blok

    struct heap_mem *first = (struct heap_mem *)((char *)tmp + sizeof(struct heap_mem));
    heapMem->next = first;
    heapMem->checksum = checksum( (void *) heapMem);

    first->prev = heapMem;
    first->size = -(int)(PAGE_SIZE - 3* sizeof(struct heap_mem));


    struct heap_mem *rightBlock = (struct heap_mem *) ((char *) first +  -first->size +  sizeof(struct heap_mem) );
    first->next = rightBlock;
    first->checksum = checksum( (void *) first);

    rightBlock->prev = first;
    rightBlock->size = 0;
    rightBlock->next= NULL;
    rightBlock->checksum = checksum( (void *) rightBlock);




    return 0;
}
void heap_clean(void)
{

    if(heapMem == NULL) return;
    struct heap_mem *node = heapMem;
    long memoryToFree = (long)((char *) custom_sbrk(0) - (char *) node);
    long heapSize = ((memoryToFree/PAGE_SIZE) + ((memoryToFree%PAGE_SIZE) != 0 ? 1 :0))*PAGE_SIZE;

    custom_sbrk(-heapSize);
    heapMem = NULL;
}


void merge_memory()
{

    struct heap_mem *node = heapMem;
    while (node!=NULL)
    {
        if(node->prev!=NULL)
        {
            if(node->prev->size<0 && node->size<0)
            {
                struct heap_mem *prevNode = node->prev;
                prevNode->next = node->next;
                prevNode->size += node->size;
                prevNode->checksum = checksum((void *)prevNode);
                struct heap_mem *nextNode = node->next;
                nextNode->prev = prevNode;
                nextNode->checksum = checksum((void *)nextNode);
            }
        }
       node = node->next;
    }

}
int ask_about_memory(void *ptr,int64_t size)
{
    struct heap_mem *node = (struct heap_mem *) ptr;
    unsigned long long number_of_pages = (size+2*FENCE_SIZE + sizeof(struct heap_mem))/PAGE_SIZE;
    // sprawdzenie czy da sie zoptymalizowac ilosc potrzebnej pamieci przez scalanie blokow

    // proszenie system o pamiec
    if(size%PAGE_SIZE!=0) number_of_pages++;
    void *newPage = custom_sbrk((int)number_of_pages*PAGE_SIZE);
    if(newPage == (void *) -1) return -1;


    // przesuniecie prawego ogranicznika na koniec bloku pamieci
    // możliwe że musi być -1 jeszcze
    struct heap_mem * rightBlock = (struct heap_mem *) ((char *)custom_sbrk(0) - (sizeof(struct heap_mem)));
    rightBlock->size=0;
    rightBlock->prev = node;
    rightBlock->next = NULL;
    rightBlock->checksum = checksum((void *) rightBlock);

    // sprawdzenie czy da sie scalic bloki
    struct heap_mem *prevNode = node->prev;
    if(prevNode!=NULL)
    {
        // jesli sie da to scalamy
        if(prevNode->size<0)
        {
            node->next = rightBlock;
            node->size = - (int64_t)(number_of_pages *PAGE_SIZE);
            node->checksum = checksum((void *) node);
            merge_memory();
            node = prevNode;
            node->next = rightBlock;
            node->checksum = checksum((void *) node);
        } // w przeciwnym wypadku tworzy sie nowy blok
        else
        {
            node->size = - (int64_t)(number_of_pages *PAGE_SIZE - sizeof(struct heap_mem));
            node->next = rightBlock;
            node->checksum = checksum((void *) node);
        }
    }
    else // to samo co powyzej - dodatkowe zabezpieczenie
    {
        node->size = - (int64_t)(number_of_pages *PAGE_SIZE - sizeof(struct heap_mem));
        node->next = rightBlock;
        node->checksum = checksum((void *) node);
    }
    return 0;
}


int heap_validate(void)
{   if(heapMem == NULL) return 2;
    struct heap_mem *node = (struct heap_mem *) heapMem;
    if(checksum((void *) node) != node->checksum) return 3;
    if(node->prev != NULL || node->size!=0 || node->next< (struct heap_mem *)((char *) node + sizeof(struct heap_mem))) return 3;
    node = node->next;
    while (node!=NULL)
    {
        if(checksum((void *) node) != node->checksum) return 3;
        if(node->prev == NULL) return 3;
        if(node->size>0)
        {
            if(memcmp((char *) node+ sizeof(struct heap_mem),"########",8 )!=0) return 1;
            if(memcmp((char *) node+ sizeof(struct heap_mem)+FENCE_SIZE+node->size,"########",8 )!=0) return 1;
        }
        node = node->next;
    }
    return 0;
}
enum pointer_type_t get_pointer_type(const void* const pointer)
{

    if(pointer == NULL) return pointer_null;
    if(heap_validate() == 1) return pointer_heap_corrupted;

    char *ptr = (char *) pointer;
    char  *header_start;
    char *header_end;
    struct heap_mem *node = (struct heap_mem *)heapMem;
    if(ptr< (char *) heapMem) return pointer_unallocated;

    while (node!=NULL) {

        header_start = (char *) node;
        header_end =  (char *)node + sizeof(struct heap_mem) - 1;


        if(header_start  <= ptr && ptr<= header_end) return pointer_control_block;
        else if(node->size>0)
        {
            if(ptr>header_end && ptr<=header_end+FENCE_SIZE  ||
               ptr>header_end+FENCE_SIZE+node->size && ptr<=header_end+FENCE_SIZE+node->size+FENCE_SIZE)
                return pointer_inside_fences;
            else if(ptr>header_end+FENCE_SIZE+1 && ptr<=header_end+FENCE_SIZE+node->size) return pointer_inside_data_block;
            else if(ptr == header_end+FENCE_SIZE+1) return pointer_valid;
            if(node->size%8 != 0)
            {
                int leftSpace = (int) node->size%8;
                if(ptr>header_end+FENCE_SIZE+node->size+FENCE_SIZE && ptr<=header_end+FENCE_SIZE+node->size+FENCE_SIZE+leftSpace)
                    return pointer_unallocated;
            }

        }
        else
        {
            if(ptr> header_end  && ptr<=header_end + -node->size)  return pointer_unallocated;
        }

        node = node->next;
    }


    return pointer_heap_corrupted;
}
void  heap_free(void* memblock)
{
    if(memblock==NULL) return;
    if(get_pointer_type(memblock) != pointer_valid) return;
    char *ptr = (char *) memblock;

    struct heap_mem *node = (struct heap_mem *) heapMem;
    char *tmp = (char *) node;
    while (tmp<ptr)
    {
        tmp = (char *) node;
        if(node->size>0)
        {
            if( tmp+ sizeof(struct heap_mem) + FENCE_SIZE == ptr)
            {
                int64_t  size = (int64_t)((char *) node->next - (char *)node - sizeof(struct heap_mem));
                node->size = -size;
                if(node->prev!= NULL)
                {
                    struct heap_mem *head = heapMem;
                    while (head!=NULL)
                    {

                        if(head->size<0 && head->prev!=NULL)
                        {
                            if(head->prev->size<0)
                            {
                                struct heap_mem *prevNode = head->prev;
                                prevNode->next = head->next;



                                head->next->prev = prevNode;
                                prevNode->size += head->size;
                                prevNode->size -= sizeof(struct heap_mem);
                                prevNode->checksum = checksum((void *) prevNode);
                                head->next->checksum = checksum((void *) head->next);
                            }
                        }
                        head  = head->next;
                    }

                }
                node->checksum = checksum((void *) node);
                return;
            }
        }
        node = node->next;
    }
}
void* heap_malloc(size_t size)
{
    if(size<1 || heapMem==NULL) return NULL;

    // szukanie wolnego miejsca aby dodac pamiec
    struct heap_mem *node = heapMem;
    while (node->next != NULL)
    {
        if(-node->size>= (int)(size + 2*FENCE_SIZE)) break;
        node = node->next;
    }

    // jesli nie ma wolnego bloku prosimy system o pamiec
    if(node->next==NULL)
    {
        // sprawdzenie ile pamieci potrzebujemy w najgorszym wypadku
        unsigned long long number_of_pages = (size+2*FENCE_SIZE + sizeof(struct heap_mem))/PAGE_SIZE;
        // sprawdzenie czy da sie zoptymalizowac ilosc potrzebnej pamieci przez scalanie blokow
        if(node->prev != NULL)
        {
            if(node->prev->size<0 && -node->prev->size >=2*FENCE_SIZE)
            {
                number_of_pages = size/PAGE_SIZE;
            }
        }


        // proszenie system o pamiec
        if(size%PAGE_SIZE!=0) number_of_pages++;
        void *newPage = custom_sbrk((int)number_of_pages*PAGE_SIZE);
        if(newPage == (void *) -1) return NULL;


        // przesuniecie prawego ogranicznika na koniec bloku pamieci
        // możliwe że musi być -1 jeszcze
        struct heap_mem * rightBlock = (struct heap_mem *) ((char *)custom_sbrk(0) - (sizeof(struct heap_mem)));
        rightBlock->size=0;
        rightBlock->prev = node;
        rightBlock->next = NULL;
        rightBlock->checksum = checksum((void *) rightBlock);

        // sprawdzenie czy da sie scalic bloki
        struct heap_mem *prevNode = node->prev;
        if(prevNode!=NULL)
        {
            // jesli sie da to scalamy
            if(prevNode->size<0)
            {
                node->next = rightBlock;
                node->size = - (int64_t)(number_of_pages *PAGE_SIZE);
                node->checksum = checksum((void *) node);
                merge_memory();
                node = prevNode;
                node->next = rightBlock;
                node->checksum = checksum((void *) node);
            } // w przeciwnym wypadku tworzy sie nowy blok
            else
            {
                node->size = - (int64_t)(number_of_pages *PAGE_SIZE - sizeof(struct heap_mem));
                node->next = rightBlock;
                node->checksum = checksum((void *) node);
            }

        }
        else // to samo co powyzej - dodatkowe zabezpieczenie
        {
            node->size = - (int64_t)(number_of_pages *PAGE_SIZE - sizeof(struct heap_mem));
            node->next = rightBlock;
            node->checksum = checksum((void *) node);
        }
    }


    memset((char *)node + sizeof(struct heap_mem)  ,'#', FENCE_SIZE); // plotek pierwszy

    memset((char *)node + sizeof(struct heap_mem)+FENCE_SIZE+(int)size,'#', FENCE_SIZE); // plotek drugi



    long BlockFreeSpace = node->size;

    // ile wielokrotnosci 8 zajmie nowy blok
    int tmp = (size % 8 == 0 ? 0: 1);
    tmp += (int)size/8;


    int BlockNewSpace = 2*FENCE_SIZE +( tmp *8);
    node->size = (int) size;
    if(-BlockFreeSpace - BlockNewSpace >= (int)sizeof(struct heap_mem)  )
    {
        struct heap_mem *newNode =  (struct heap_mem *)((char *)node + sizeof(struct heap_mem) + BlockNewSpace);
        struct heap_mem *nextNode  = node->next;


        newNode->next = nextNode;
        newNode->prev = node;
        newNode->size = (int)(BlockFreeSpace + BlockNewSpace + sizeof(struct heap_mem));
        newNode->checksum = checksum((void *) newNode);


        nextNode->prev= newNode;
        nextNode->checksum = checksum((void *) nextNode);
        node->next = newNode;
        node->checksum = checksum((void *)  node);

    }
    node->checksum = checksum((void *) node);


    return(void *) ((char *)node + sizeof(struct heap_mem)+FENCE_SIZE);
}
void* heap_calloc(size_t number, size_t size)
{
    if(number<1 || size<1) return NULL;
    void *ptr = heap_malloc(number*size);
    if(ptr==NULL) return NULL;
    uint8_t  *tmp = (uint8_t *) ptr;
    for(size_t i=0; i<number*size; i++)
    {
        *(tmp+i) = 0;
    }
    return ptr;
}
size_t heap_get_largest_used_block_size(void)
{
    if(heapMem == NULL) return 0;
    if(heap_validate()!=0) return 0;
    struct heap_mem *node = (struct heap_mem *)heapMem;
    int64_t max=0;
    while (node!=NULL)
    {
        if(node->size > max) max =node->size;
        node = node->next;
    }
    return max;
}
void* heap_realloc(void* memblock, size_t count)
{
    if(get_pointer_type(memblock)!=pointer_valid && memblock!=NULL) return NULL;
    if(memblock==NULL) return heap_malloc(count);
    char * ptr = (char *) memblock;
    struct heap_mem *allocatedBlock =(struct heap_mem *) (ptr - FENCE_SIZE - sizeof(struct heap_mem));
    if(allocatedBlock->size ==   (int64_t) count) return memblock;

    // zmienijaszanie rozmiaru
    if(allocatedBlock->size > (int64_t) count)
    {
        if(count == 0)
        {
            heap_free(memblock);
            return NULL;
        }

        // wyliczenie ile bajtow zajmuje nowy blok zaokraglone do wielokrotnosci 8
        int64_t tmp = (int64_t)count/8 + (count%8!=0 ? 1:0);
        int64_t newSize = tmp * 8;



        int64_t blocksSpace = (int64_t) (allocatedBlock->size);
        int64_t LeftSpace = blocksSpace - newSize;

        memset((char *)allocatedBlock + sizeof(struct heap_mem)+FENCE_SIZE+(int)count,'#', FENCE_SIZE); // plotek drugi

        // jesli mamy miejsce to tworzymy nowy naglowek a jesli nie to tylko zaktualizujemy rozmair i sume kontrolna
      //  if(allocatedBlock->size - newSize >= (int64_t)sizeof(struct heap_mem))
        if (LeftSpace >= (int64_t) sizeof(struct heap_mem))
        {
              struct heap_mem *nextNode  = (struct heap_mem *)((char *)allocatedBlock->next - LeftSpace);
              nextNode->size = -(LeftSpace - (int64_t) sizeof(struct heap_mem));
              nextNode->next = allocatedBlock->next;
              nextNode->prev = allocatedBlock;
              nextNode->checksum = checksum((void *) nextNode);


              allocatedBlock->next->prev = nextNode;
              allocatedBlock->next->checksum = checksum((void *) allocatedBlock->next);

              allocatedBlock->next = nextNode;
        }

        allocatedBlock->size = (int64_t)count;
        allocatedBlock->checksum = checksum((void *)  allocatedBlock );

      return memblock;
    }

    if(allocatedBlock->prev!=NULL && allocatedBlock->next!=NULL)
    {
        struct heap_mem *prevNode = allocatedBlock->prev;
        struct heap_mem *nextNode = allocatedBlock->next;

        // jeśli jest połączenie dwóch bloków lewego i obecnego
        if (prevNode->size < 0 &&
            (int64_t) (-prevNode->size + allocatedBlock->size + sizeof(struct heap_mem)) >= (int64_t) count) {
            char *pointer_to_data = (char *) allocatedBlock + sizeof(struct heap_mem) + FENCE_SIZE;
            char *pointer_to_new_data = (char *) prevNode + sizeof(struct heap_mem) + FENCE_SIZE;
            struct heap_mem dummy;
            dummy.size = allocatedBlock->size;
            dummy.next = allocatedBlock->next;
            dummy.prev = allocatedBlock->prev;

            int64_t blocksSpace = (int64_t) (dummy.size + (int64_t) sizeof(struct heap_mem) + (-prevNode->size));

            memset((char *) prevNode + sizeof(struct heap_mem), '#', FENCE_SIZE); // plotek pierwszy
            memcpy(pointer_to_new_data, pointer_to_data, dummy.size); // kopiowanie danych
            memset((char *) prevNode + sizeof(struct heap_mem) + FENCE_SIZE + (int) count, '#',
                   FENCE_SIZE); // plotek drugi


            prevNode->next = nextNode;
            prevNode->size = (int64_t) count;
            prevNode->checksum = checksum((void *) prevNode);
            nextNode->prev = prevNode;
            nextNode->checksum = checksum((void *) nextNode);


            int64_t tmp = (int64_t) count / 8 + (count % 8 != 0 ? 1 : 0);
            int64_t newSize = tmp * 8;

            int64_t LeftSpace = blocksSpace - newSize;
            // jesli da sie utworzyc naglowek
            if (LeftSpace >= (int64_t) sizeof(struct heap_mem)) {
                struct heap_mem *newNode = (struct heap_mem *) ((char *) nextNode - LeftSpace);
                newNode->prev = prevNode;
                newNode->next = nextNode;
                newNode->size = -(LeftSpace - (int64_t) sizeof(struct heap_mem));
                newNode->checksum = checksum((void *) newNode);

                prevNode->next = newNode;
                prevNode->checksum = checksum((void *) prevNode);

                nextNode->prev = newNode;
                nextNode->checksum = checksum((void *) nextNode);
            }


            return pointer_to_new_data;
        }
        // jesli jest mozliwosc połoczenia z prawym blokiem
        if (nextNode->size < 0 &&
            (int64_t) (-nextNode->size + allocatedBlock->size + sizeof(struct heap_mem)) >= (int64_t) count) {

            int64_t blocksSpace = (int64_t) (allocatedBlock->size + (int64_t) sizeof(struct heap_mem) +
                                             (-nextNode->size));

            struct heap_mem dummy;
            dummy.prev = allocatedBlock->prev;
            dummy.next = nextNode->next;


            memset((char *) allocatedBlock + sizeof(struct heap_mem), '#',
                   FENCE_SIZE); // plotek pierwszy - to w sumie nie jest potrzebne bo już jest
            memset((char *) allocatedBlock + sizeof(struct heap_mem) + FENCE_SIZE + (int) count, '#',
                   FENCE_SIZE); // plotek drugi

            allocatedBlock->prev = dummy.prev;
            allocatedBlock->next = dummy.next;
            allocatedBlock->size = (int64_t) count;
            allocatedBlock->checksum = checksum((void *) allocatedBlock);
            struct heap_mem *temp = dummy.next;
            if (temp != NULL) {
                temp->prev = allocatedBlock;
                temp->checksum = checksum((void *) temp);
                nextNode = temp;
            }

            int64_t tmp = (int64_t) count / 8 + (count % 8 != 0 ? 1 : 0);
            int64_t newSize = tmp * 8;

            int64_t LeftSpace = blocksSpace - newSize;
            // jesli da sie utworzyc naglowek
            if (LeftSpace >= (int64_t) sizeof(struct heap_mem)) {
                struct heap_mem *newNode = (struct heap_mem *) ((char *) nextNode - LeftSpace);
                newNode->prev = allocatedBlock;
                newNode->next = nextNode;
                newNode->size = -(LeftSpace - (int64_t) sizeof(struct heap_mem));
                newNode->checksum = checksum((void *) newNode);

                allocatedBlock->next = newNode;
                allocatedBlock->checksum = checksum((void *) allocatedBlock);

                nextNode->prev = newNode;
                nextNode->checksum = checksum((void *) nextNode);
            }

            return memblock;
        }

       //łączenie 3 blokoów
        if (nextNode->size < 0 && prevNode->size < 0 &&
            (int64_t) (-nextNode->size + (-prevNode->size) + allocatedBlock->size + 2 * sizeof(struct heap_mem)) >=
            (int64_t) count) {

            char *pointer_to_data = (char *) allocatedBlock + sizeof(struct heap_mem) + FENCE_SIZE;
            char *pointer_to_new_data = (char *) prevNode + sizeof(struct heap_mem) + FENCE_SIZE;
            int64_t blocksSize = (int64_t) (-nextNode->size + (-prevNode->size) + allocatedBlock->size +
                                            2 * sizeof(struct heap_mem));

            struct heap_mem dummy;
            if (nextNode->next == NULL) return NULL;
            dummy.next = nextNode->next;
            dummy.prev = prevNode;


            memset((char *) prevNode + sizeof(struct heap_mem), '#', FENCE_SIZE); // plotek pierwszy
            memcpy(pointer_to_new_data, pointer_to_data, allocatedBlock->size); // kopiowanie danych
            memset((char *) prevNode + sizeof(struct heap_mem) + FENCE_SIZE + (int) count, '#',
                   FENCE_SIZE); // plotek drugi


            prevNode->size = (int64_t) count;
            prevNode->next = dummy.next;
            prevNode->checksum = checksum((void *) prevNode);


            prevNode->next->prev = prevNode;
            prevNode->next->checksum = checksum((void *) prevNode->next);


            int64_t tmp = (int64_t) count / 8 + (count % 8 != 0 ? 1 : 0);
            int64_t newSize = tmp * 8;
            int64_t LeftSpace = blocksSize - newSize;
            nextNode = nextNode->next;
            // jesli da sie utworzyc naglowek
            if (LeftSpace >= (int64_t) sizeof(struct heap_mem)) {
                struct heap_mem *newNode = (struct heap_mem *) ((char *) dummy.next - LeftSpace);
                newNode->prev = dummy.prev;
                newNode->next = dummy.next;
                newNode->size = -(LeftSpace - (int64_t) sizeof(struct heap_mem));
                newNode->checksum = checksum((void *) newNode);

                prevNode->next = newNode;
                prevNode->checksum = checksum((void *)  prevNode);

                newNode->next->prev = newNode;
                newNode->next->checksum = checksum((void *) newNode->next);
            }
            return (void *) pointer_to_new_data;
        }
        // to samo tylko że na końcu - wtedy rekurencja
        if (nextNode->size < 0 && prevNode->size < 0 &&
            (int64_t) (-nextNode->size + (-prevNode->size) + allocatedBlock->size + 2 * sizeof(struct heap_mem)) <
            (int64_t) count) {
            if (nextNode->next == NULL) return NULL;
            if (nextNode->next->size == 0) {
                int64_t neededSize = (int64_t)count - (int64_t) (-nextNode->size + (-prevNode->size) + allocatedBlock->size +2 * sizeof(struct heap_mem));
                int r = ask_about_memory((void *)nextNode->next, neededSize);
                if(r!=0) return NULL;
                return heap_realloc(memblock, count);
            }

        }

        // jeśli dodajemy na końcu
        if (nextNode->size == 0) {
            size_t neededSize = count - allocatedBlock->size;
            int r = ask_about_memory((void *)nextNode, (int64_t)neededSize);
            if (r != 0) return NULL;
            return heap_realloc(memblock, count);
        }

        else {
            struct heap_mem *node = heapMem;
            while (node->next->size != 0) {
                if (-node->size  >=  (int64_t)count + 2 * FENCE_SIZE) break;
                node = node->next;
            }

            if (node->next->size == 0 && node->size < 0 && -node->size <  (int64_t)count)
            {
                int64_t neededSize = (int64_t) count - (-node->size);
                int r = ask_about_memory((void *) node->next, neededSize);
                if(r!=0) return NULL;
                return heap_realloc(memblock, count);
            }
            else if (node->size > 0 && node->next->size == 0)
            {
                char *p =  heap_malloc(count);
                if(p==NULL) return NULL;
                memcpy(p,memblock,allocatedBlock->size);
                heap_free(memblock);
                return p;

            }
            else
            {
                char *new_data_pointer = (char *) node + sizeof(struct heap_mem) + FENCE_SIZE;
                int64_t blockSpace = -node->size - 2*FENCE_SIZE;
                memset((char *) node + sizeof(struct heap_mem), '#', FENCE_SIZE); // plotek pierwszy
                memcpy(new_data_pointer, memblock, allocatedBlock->size);
                memset((char *) node + sizeof(struct heap_mem) + FENCE_SIZE + (int) count, '#',
                       FENCE_SIZE); // plotek drugi



                node->size = (int64_t) count;
                node->checksum = checksum((void *) node);


                int64_t tmp = (int64_t) count / 8 + (count % 8 != 0 ? 1 : 0);
                int64_t newSize = tmp * 8;
                int64_t LeftSpace = blockSpace - newSize;

                // jesli da sie utworzyc naglowek
                struct heap_mem *nextNodeTmp = node->next;
                if (LeftSpace  >= (int64_t) sizeof(struct heap_mem)) {
                    struct heap_mem *newNode = (struct heap_mem *) ((char *) nextNodeTmp - LeftSpace);
                    newNode->prev = node;
                    newNode->next = nextNodeTmp;
                    newNode->size = -(LeftSpace - (int64_t) sizeof(struct heap_mem));
                    newNode->checksum = checksum((void *) newNode);

                    node->next = newNode;
                    node->checksum = checksum((void *) node);

                    nextNodeTmp->prev = newNode;
                    nextNodeTmp->checksum = checksum((void *) nextNodeTmp);
                }

                heap_free(memblock);
                return new_data_pointer;
            }
        }
    }


    return NULL;
}