#include "library.h"
// #include <stdio.h>
#include <string.h>
#include <unistd.h>

// region Params and Struct Defs

// Normally I'd write a test suite for this, but I don't know how in C, let alone the countless other syntax errors that are here.

// Done: Make Upper Order memory in split be the active memory so you don't have to update the free list
// DONE: Move HeapHeader to BSS, (makes program efficient, increases usable page size, makes less likely to be trashed by user (more robust))

// Since we are writing a memory management package should all of these definition be within the heap-header or at least the variables?
// Or should we let this stuff exist in the BSS instead?
typedef struct ListNode ListNode;
struct ListNode {
    ListNode *next;
    ListNode *prev;
};

typedef struct HeapHeader HeapHeader;
struct HeapHeader {
    ListNode *head;
    ListNode *tail;
};

typedef struct TwoAddresses TwoAddresses;
struct TwoAddresses {
    void *a;
    void *b;
};

typedef struct BlockInfo BlockInfo;
struct BlockInfo {
    int *start;
    int *end;
    int size;
    int flag;
};

enum Flags {
    inactive, // 0
    active, // 1
};

const int word_size = sizeof(void*);
const int minimum_alloc = word_size * 3;
HeapHeader heap_header = (struct HeapHeader){.head = 0, .tail = 0};
const int max = 4096;
const int minimum_valid_block_size = word_size * 5;

char *memory_start = 0;
char *memory_end = 0;

//endregion


// region Flagstuff
int readFlag(const int value) {
    return value & 0b111;
}

int flaglessSize(const int value) {
    // this works because 7 is 0000000111 but the 0s will flip also because the int is always at a fixed size because why not
    // these abstractions obfuscate what is actually going on I genuinely feel it'd be easier to write this in assembly
    return value & ~0b111;
}

void writeFlag(int *addressValue, const int flag) {
    *addressValue = flaglessSize(*addressValue);
    *addressValue = *addressValue + flag;
}

//endregion


// region Free List Manipulation
void addBlockToFreeList(void *addressOfUsableMemoryInNewFreeBlock) {
    // Update the tail at the head
    // Use the previous tail to define the previous node

    // Create an address which is the address of the previous tail
    ListNode *previousLastNode = heap_header.tail;
    // Create a ListNode at the address of the usable memory in the last free block
    // addressOfUsableMemoryInNewFreeBlock = (ListNode){0, previousLastNode};
    *(ListNode *)addressOfUsableMemoryInNewFreeBlock = (ListNode){0, previousLastNode};
    // Assign the tail for the value stored at the starting_place (the HeapHeader) to be the address of the last free block
    heap_header.tail = addressOfUsableMemoryInNewFreeBlock;
}

void removeBlockFromFreeList(ListNode *blockToRemove) {
    // If it is the head we must update the head
    // If it is the tail we must update the tail
    // If it is in the middle we need to update the prev and next references to each other
    if (blockToRemove->prev == 0) {
        heap_header.head = blockToRemove->next;
    }
    if (blockToRemove->next == 0) {
        heap_header.tail = blockToRemove->prev;
    }
    // The previous Node needs to point to the next node
    *blockToRemove->prev->next = *blockToRemove->next;
    // The next node needs to trail back to the previous node
    *blockToRemove->next->prev = *blockToRemove->prev;

    // This is how you reset a range of memory apparently in C, however I couldn't find a function definition
    // For apparently being a low level language not much you can do by hand, I feel like I still have to rely on
    // wierd APIs which is what I was trying to escape
    memset(blockToRemove, 0, sizeof(*blockToRemove));

}

ListNode *checkFreeListForBlock(const int size) {
    // If the freelist is empty return nullptr
    if (heap_header.head == 0){return 0;}
    // For every node in the freelist

    // [SRM3] Use pointers (to ListNode) here, not ListNode's, themselves.

    for (ListNode *node = heap_header.head; node->next != 0; node = node->next) {
        // If the block size (the flagless value at the preceding address) is greater than the given size
        // Flags can matter in case the sizes are equal (even though the free flag is 0)
        if (flaglessSize(*(int *)((char *) node - word_size)) >= size) {
            // Return the address of the node (which is the start of the free block)
            return node;
        }
    }
    return 0;
}
// endregion


// region Utility Functions
BlockInfo verifyBlock(void *address_of_usable_memory) {
    // Get the address of the header (address right before this address)
    int *start = (int *) ((char *) address_of_usable_memory - word_size);
    //  and values of the header
    const int size = flaglessSize(*start);
    const int flag = readFlag(*start);
    // Get address of footer
    int *end = (int *) ((char *) address_of_usable_memory + size);
    if (*start != *end || size < minimum_alloc || (char *) end > memory_end || start > end) {
        // This is how I am returning invalid blocks
        return (BlockInfo){.start=0, .end=0, .size=0, .flag=0};
    }
    return (BlockInfo){.start=start, .end=end, .size=size, .flag=flag};
}

//endregion


// region Allocation Stuff

TwoAddresses create_footer_and_header(int *headerStart, const int size, const int flag) {
    // Create header and footer
    // Return the size

    // Assign the size to address start
    *headerStart = size + flag;

    // Calculate end and assign size
    int *end = headerStart + word_size + size;
    *end = size + flag;

    const TwoAddresses result = {headerStart,end};
    return result;
}

void splitBlock(ListNode* startAddressOfUsableMemory, int size) {
    // Done: Refactor split to split from the bottom to not have to do a freelist manipulation

    // Define initial parameters:
    BlockInfo initial_block_info = verifyBlock(startAddressOfUsableMemory);
    // int initialBlockEnd = flaglessSize(*(int *)((char *)startAddressOfUsableMemory + initialBlockSize));
    int free_block_size = initial_block_info.size - size;
    if (free_block_size + (2 * word_size) < minimum_valid_block_size) {
        // size = size + free_block_size;
        free_block_size = 0;
    }
    // int *activeBlockEnd = initial_block_info.end;

    // If the whole block is being allotted just remove it from free list and rewrite as active
    if (free_block_size == 0) {
        removeBlockFromFreeList(startAddressOfUsableMemory);
        writeFlag(initial_block_info.start, active);
        writeFlag(initial_block_info.end, active);
        return;
    }
    // If not define the active block and create it
    int *activeBlockStart = initial_block_info.end - size;
    create_footer_and_header(activeBlockStart, size, active);

    // Then define the free block and create it
    int *free_block_start = initial_block_info.start;
    create_footer_and_header(free_block_start, free_block_size, inactive);

    // Since I made the upper memory the free memory no need to manipulate free block entries since they already exist

    // addBlockToFreeList(free_block_start + word_size);

}

bool sizeIsValid(const int size) {
    if (size < minimum_alloc){return false;}
    // Is not divisible by 8
    if (readFlag(size) != 0) {
        return false;
    }
    return true;
}

int adjustToNearestValidSize(const int size) {
    if (size < minimum_alloc) {
        return minimum_alloc;
    }
    // Is above minimum and is divisible by 8 (it is valid)
    if (readFlag(size) == 0) {
        return size;
    }
    // If it is not we snap it to the nearest largest number divisible by 8
    return flaglessSize(size) + 8;

}

void *allocate(int size) {
    // if (~sizeIsValid(size)){return 0;}
    size = adjustToNearestValidSize(size);
    ListNode *nodeOfFreeGuy = checkFreeListForBlock(size);
    if (nodeOfFreeGuy == 0){return 0;}
    splitBlock(nodeOfFreeGuy, size);
    // Return the address (since the address of the )

    // [SRM3] nodeOfFreeGuy is a pointer; return that value, rather than the address of the pointer!

    return (void *) nodeOfFreeGuy;
}

//endregion


// region Free Stuff
bool blockIsInvalid(const BlockInfo block) {
    // This is the definition of an incorrect block
    if (block.start == 0 && block.end == 0) {
        return true;
    }
    return false;
}

bool blockIsValid(const BlockInfo block_to_free) {
    return !blockIsInvalid(block_to_free);
}

void double_coalesce(BlockInfo freed_block, BlockInfo previous_block, BlockInfo next_block) {
    int size = &next_block.end - &previous_block.start;
    removeBlockFromFreeList((ListNode *)(next_block.start + word_size));
    *previous_block.start = size;
    *previous_block.end = 0;
    *freed_block.start = 0;
    *freed_block.end = 0;
    *next_block.start = 0;
    *next_block.end = size;
    // Freelist entry already exists
    // addBlockToFreeList(&previous_block.start + word_size);
}
void lower_coalesce(BlockInfo freed_block, BlockInfo next_block) {
    int size = &next_block.end - &freed_block.start;
    removeBlockFromFreeList((ListNode *)(next_block.start + word_size));
    *freed_block.start = size;
    *freed_block.end = 0;
    *next_block.start = 0;
    *next_block.end = size;
    addBlockToFreeList(&freed_block.start + word_size);

}
void upper_coalesce(BlockInfo freed_block, BlockInfo previous_block) {
    int size = &freed_block.end - &previous_block.start;
    // Freed block was never initially added to freelist just memset 0 and flag rewrites
    // removeBlockFromFreeList((ListNode *)(next_block.start + word_size));
    *previous_block.start = size;
    *previous_block.end = 0;
    *freed_block.start = 0;
    *freed_block.end = size;
    //Entry on previous block already exists
    // addBlockToFreeList(&previous_block.start + word_size);
}
void coalesce(BlockInfo freed_block, BlockInfo previous_block, BlockInfo next_block) {
    // Add conditions for if there is no previous or next block due to it being first or last
    if (next_block.flag == active && previous_block.flag == active) {
        addBlockToFreeList(&freed_block.start + word_size);
        return;
    }
    if (blockIsInvalid(previous_block) && blockIsInvalid(next_block)) {
        addBlockToFreeList(&freed_block.start + word_size);
        return;
    }
    if (next_block.flag == inactive && previous_block.flag == inactive && blockIsValid(next_block) && blockIsValid(previous_block)) {
        double_coalesce(freed_block, previous_block, next_block);
        return;
    }
    if (next_block.flag == inactive && (previous_block.flag == active || blockIsInvalid(previous_block))) {
        lower_coalesce(freed_block, next_block);
        return;
    }
    if ((next_block.flag == active || blockIsInvalid(next_block)) && previous_block.flag == inactive) {
        upper_coalesce(freed_block, previous_block);
        // return;
    }
}

void clean_freed_address(void *address_of_usable_memory, BlockInfo freed_block) {
    writeFlag(freed_block.start, inactive);
    writeFlag(freed_block.end, inactive);
    // Delete memory that was previously there for security concerns
    memset(address_of_usable_memory, 0, flaglessSize(freed_block.size));
}

bool free_address(void *address_of_usable_memory) {
    /* Detect which adjacent blocks are empty
     * If next is empty delete it from the free list
     * If last is not empty add current to free list
     * calculate size of new free block
     * delete all headers
     * write where new headers are supposed to be
    */
    const BlockInfo block_to_free = verifyBlock(address_of_usable_memory);
    if (blockIsInvalid(block_to_free)){return false;}
    if (block_to_free.flag == inactive){return false;}
    // DONE: IF IT IS THE FIRST OR LAST BLOCK THERE IS NO PREV OR NEXT (Factor this in accordingly check that I check for it)
    const BlockInfo previous_block = verifyBlock(block_to_free.start - word_size);
    const BlockInfo next_block = verifyBlock(block_to_free.end + word_size);
    clean_freed_address(address_of_usable_memory, block_to_free);
    coalesce(block_to_free, previous_block, next_block);
    return true;
    // if (blockIsInvalid(block_to_free)){return false;}
}

//endregion


// region System wide funcs
// DONE: Rewrite given the fact that HeapHeader was moved to BSS
void initializeHeap() {
    memory_start = sbrk(max);
    memory_end = memory_start + max;

    create_footer_and_header((int *) memory_start, max - word_size, inactive);
    // Define the address where the usable memory block starts (bc above func returns ptr to header)

    // Since we initialize this as a free block we include it in the free list
    *(ListNode *)(memory_start + word_size) = (ListNode){0, 0};

    heap_header.head = heap_header.tail = (ListNode *)(memory_start + word_size);
}
typedef struct BoolAndCount BoolAndCount;
struct BoolAndCount {
    bool bool_value;
    int count_or_address;
};

BoolAndCount verifyHeapIntegrity() {
    /* Walk Free List
     * Verify one block at a time
     */

    // If false will return with the address of the compromised block
    // If true will return with the count of free blocks from the walk

    int count_free_blocks = 0;
    for (
	 // The size of HeapHeader + 1 address (first block header)
	 // will give you the start of the usable memory there

	 // Done: Rewrite given the fact that HeapHeader was moved to BSS
	 // START CONDITION
	 BlockInfo current_block = verifyBlock(memory_start + word_size);

	 // The address where the last block ends should always be the
	 // last data in general, as such accessing the "start" of the
	 // last address + its size should == the maximum space

	 // END CONDITION

	 // [SRM3] This condition isn't being used to make any choices,
	 // so what are you intending?

	 (char *) current_block.end + word_size == memory_end;

	 // ITERATOR
	 current_block = verifyBlock(current_block.end + 2 * word_size)
        )
    {
        if (blockIsInvalid(current_block)) {
            return (BoolAndCount){false, *current_block.start + word_size};
        }
        if (current_block.flag == inactive) {
            count_free_blocks++;
        }
    }
    return (BoolAndCount){true, count_free_blocks};
}

bool verifyFreeList(const int free_list_count) {
    // This is not how we discussed verifying the free list however I implemented the freelist to be non-sequential in
    // terms of address, rather it is based off the last added block. As such I need a different method to verify the free
    // list since walking the blocks won't ensure a sequential walk of the next free block

    // Will return False if the free list is invalid (and true if it is valid)

    int list_cnt = 0;
    const ListNode *last_node = &(struct ListNode){0, 0};
    for (ListNode current_node = *heap_header.head; current_node.next == 0; current_node = *current_node.next) {
        list_cnt++;
        last_node = &current_node;
        // if (current_node.header != starting_place) {
        //     return false;
        // }
    }
    if (free_list_count != list_cnt) {
        return false;
    }
    if (last_node->next == 0 && last_node->prev == 0 && list_cnt == 0) {
        return true;
    }
    if (last_node != heap_header.tail) {
        return false;
    }
    return true;
}

BoolAndCount verify() {
    const BoolAndCount heapIntegrityResult = verifyHeapIntegrity();
    if (~heapIntegrityResult.bool_value){return heapIntegrityResult;}
    bool const freeListResult = verifyFreeList(heapIntegrityResult.count_or_address);
    // If FreeList is invalid this will return False and the lack of an address will indicate it's a Free list error
    return (BoolAndCount){freeListResult, 0};

}

// Other system func Ideas, get_active_block_count, get_active_block_addresses and sizes

// For get active block addresses there can be any number of active block addresses as such I don't know how I'd be able
// to return that efficiently.

//endregion
