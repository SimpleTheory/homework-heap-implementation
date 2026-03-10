#include "library.h"
// #include <stdio.h>
#include <string.h>

// region Params and Struct Defs

// Normally I'd write a test suite for this, but I don't know how in C, let alone the countless other syntax errors that are here.

// TODO: Make Upper Order memory in split be the active memory so you dont have to update the free list
// TODO: Move HeapHeader to BSS, (makes program efficient, increases usable page size, makes less likely to be trashed by user (more robust))

// Since we are writing a memory management package should all of these definition be within the heap-header or at least the variables?
// Or should we let this stuff exist in the BSS instead?

struct ListNode {
    struct HeapHeader *header;
    struct ListNode *next;
    struct ListNode *prev;
};

struct HeapHeader {
    struct ListNode *head;
    struct ListNode *tail;
};

struct TwoAddresses {
    int *a;
    int *b;
};

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
const int starting_place = 0;
const int max = 4096;
const int minimum_valid_block_size = word_size * 5;

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
void addBlockToFreeList(int addressOfUsuableMemoryInNewFreeBlock) {
    // Update the tail at the head
    // Use the previous tail to define the previous node

    // Create an address which is the address of the previous tail
    int previousLastNode = *starting_place.tail;
    // Create a ListNode at the address of the usable memory in the last free block
    *addressOfUsuableMemoryInLastFreeBlock = (struct ListNode){*starting_place, 0, previousLastNode};
    // Assign the tail for the value stored at the starting_place (the HeapHeader) to be the address of the last free block
    *starting_place.tail = addressOfUsuableMemoryInLastFreeBlock;
}

void removeBlockFromFreeList(struct ListNode blockToRemove) {
    // If it is the head we must update the head
    // If it is the tail we must update the tail
    // If it is in the middle we need to update the prev and next references to each other
    if (blockToRemove.prev == 0) {
        *starting_place.head = blockToRemove.next;
    }
    if (blockToRemove.next == 0) {
        *starting_place.tail = blockToRemove.prev;
    }
    // The previous Node needs to point to the next node
    *blockToRemove.prev->next = *blockToRemove.next;
    // The next node needs to trail back to the previous node
    *blockToRemove.next->prev = *blockToRemove.prev;

    // This is how you reset a range of memory apparently in C, however I couldn't find a function definition
    // For apparently being a low level language not much you can do by hand, I feel like I still have to rely on
    // wierd APIs which is what I was trying to escape
    memset(&blockToRemove, 0, sizeof(blockToRemove));

}

struct ListNode* checkFreeListForBlock(const int size) {
    // If the freelist is empty return nullptr
    if (*starting_place.head == 0){return 0;}
    // For every node in the freelist
    for (struct ListNode node = starting_place.head; node.next != 0; node = *node.next) {
        // If the block size (the flagless value at the preceding address) is greater than the given size
        // Flags can matters in case the sizes are equal (even though the free flag is 0)
        if (flaglessSize(*(&node - word_size)) >= size) {
            // Return the address of the node (which is the start of the free block)
            return &node;
        }
    }
    return 0;
}
// endregion


// region Utility Functions
struct BlockInfo verifyBlock(int *address_of_usable_memory) {
    // Get the address of the header (address right before this address)
    int *start = address_of_usable_memory - word_size;
    //  and values of the header
    int size = flaglessSize(start);
    int flag = readFlag(start);
    // Get address of footer
    int *end = address_of_usable_memory + size;
    if (start != end || size < minimum_alloc || end > max || start > end) {
        // This is how I am returning invalid blocks
        return (struct BlockInfo){.start=0, .end=0, .size=0, .flag=0};
    }
    return (struct BlockInfo){.start=start, .end=end, .size=size, .flag=flag};
}

//endregion


// region Allocation Stuff

struct TwoAddresses create_footer_and_header(int *headerStart, const int size, const int flag) {
    // Create header and footer
    // Return the size

    // Assign the size to address start
    *headerStart = size + flag;

    // Calculate end and assign size
    int *end = headerStart + word_size + size;
    *end = size + flag;

    const struct TwoAddresses result = {headerStart,end};
    return result;
}

void splitBlock(struct ListNode* startAddressOfUsableMemory, int size) {
    // Get initial block sizes
    const int initialBlockSize = flaglessSize(startAddressOfUsableMemory - word_size);
    int block2Size = initialBlockSize - size;
    if (block2Size + (2 * word_size) < minimum_valid_block_size) {
        size += block2Size;
        block2Size = 0;
    }


    // Initialize addresses for the new block's headers and footers
    // (everytime I initialize addresses the IDE yells at me, so I don't even know how to type them anymore)

    block1Start = &startAddressOfUsableMemory - word_size;
    block1End = &startAddressOfUsableMemory + size;

    block2Start = block1End + word_size;
    block2End = block2Start + block2Size;

    removeBlockFromFreeList(*startAddressOfUsableMemory);
    create_footer_and_header(block1Start, size, active);
    if (block2Size == 0){return;}
    create_footer_and_header(block2Start, block2Size, inactive);
    addBlockToFreeList(block2Start + word_size);

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

int allocate(int size) {
    // if (~sizeIsValid(size)){return 0;}
    size = adjustToNearestValidSize(size);
    struct ListNode* nodeOfFreeGuy = checkFreeListForBlock(size);
    if (nodeOfFreeGuy == 0){return 0;}
    splitBlock(nodeOfFreeGuy, size);
    // Return the address (since the address of the )
    return &nodeOfFreeGuy;
}

//endregion


// region Free Stuff
bool blockIsInvalid(const struct BlockInfo block_to_free) {
    // This is the definition of an incorrect block
    if (block_to_free.start == 0 && block_to_free.end == 0) {
        return true;
    }
    return false;
}

void coalesce(struct BlockInfo freed_block, struct BlockInfo previous_block, struct BlockInfo next_block) {
    if (next_block.flag == active && previous_block.flag == active) {
        // Rewrite from active to inactive because the block starts with the freed block's header
        // Rewrite from active to inactive because the block ends with the freed block's footer
        writeFlag(freed_block.start, inactive);
        writeFlag(freed_block.end, inactive);
        return;
    }

    int size;
    int array_size;
    int *address_array[6];

    if (next_block.flag == inactive && previous_block.flag == inactive) {
        // No flag rewriting because the freed block's header and footer will be deleted
        size = previous_block.size + freed_block.size + next_block.size;
        array_size = 6;
        address_array = {previous_block.start, previous_block.end, freed_block.start, freed_block.end, next_block.start, next_block.end};

        // Arg is supposed to be address where ListNode is (address one ptr size after headers)

        // Because the freelist node of the previous block is already at the correct place then all we need to do is
        // remove the next_block's node
        removeBlockFromFreeList(*(next_block.start + word_size)); // Supposed to call on address where the ListNode would be
    }

    else if (next_block.flag == inactive) {
        // Rewrite from active to inactive because the block starts with the freed block's header
        writeFlag(freed_block.start, inactive);
        size = freed_block.size + next_block.size;
        array_size = 4;
        address_array = {freed_block.start, freed_block.end, next_block.start, next_block.end, 0 , 0};

        // Arg is supposed to be address where ListNode is (address one ptr size after headers)

        // Here we need to move the node from the next_block to the freed_block to coalesce
        removeBlockFromFreeList(*(next_block.start + word_size));
        addBlockToFreeList(*(freed_block.start + word_size));
    }
    else {
        // Rewrite from active to inactive because the block ends with the freed block's footer
        writeFlag(freed_block.end, inactive);
        size = freed_block.size + previous_block.size;
        array_size = 4;
        address_array = {previous_block.start, previous_block.end, freed_block.start, freed_block.end, 0, 0};
        // Here because there is no next no node maintenance needs to occur since the node (prev_block) is already at the correct spot
    }

    for (int index = 0; index < array_size; index++) {
        int *current_address = address_array[index];

        // The new header and footer will be the first and last item in the array
        if (index == 0 || index + 1 == array_size) {
            *current_address = size; // + inactive; Inactive is 0 so no need to add flag atm
        }
        else {
            // Delete the former headers and footers
            *current_address = 0;
        }
    }
}

bool free_address(int *address_of_usable_memory) {
    /* Detect which adjacent blocks are empty
     * If next is empty delete it from the free list
     * If last is not empty add current to free list
     * calculate size of new free block
     * delete all headers
     * write where new headers are supposed to be
    */
    const struct BlockInfo block_to_free = verifyBlock(address_of_usable_memory);
    if (blockIsInvalid(block_to_free)){return false;}
    if (block_to_free.flag == inactive){return false;}
    // TODO IF IT IS THE FIRST OR LAST BLOCK THERE IS NO PREV OR NEXT (Factor this in accordingly check that I check for it)
    const struct BlockInfo previous_block = verifyBlock(block_to_free.start - word_size);
    const struct BlockInfo next_block = verifyBlock(block_to_free.end + word_size);
    // if (blockIsInvalid(previous_block) || blockIsInvalid(next_block)){return false;}
    coalesce(block_to_free, previous_block, next_block);
    return true;
    // if (blockIsInvalid(block_to_free)){return false;}
}

//endregion


// region System wide funcs
void initializeHeap() {
    // Create Heap Header at the address starting place
    *starting_place = (struct HeapHeader){.head = 0, .tail = 0};
    const int header_size = sizeof(struct HeapHeader);
    // The size of the initial block must take into account the header-size and the size of a footer afterwards
    // because of how the function is implemented
    const struct TwoAddresses initialBlock = create_footer_and_header(header_size, max - header_size - word_size, inactive);
    // Define the address where the usable memory block starts (bc above func returns ptr to header)
    int *memory_start = initialBlock.a + word_size;
    // Since we initialize this as a free block we include it in the free list
    *memory_start = (struct ListNode){*starting_place, 0, 0};
    *starting_place.head = memory_start;
}

struct BoolAndCount {
    bool bool_value;
    int count_or_address;
};

struct BoolAndCount verifyHeapIntegrity() {
    /* Walk Free List
     * Verify one block at a time
     */

    // If false will return with the address of the compromised block
    // If true will return with the count of free blocks from the walk

    int count_free_blocks = 0;
    for (
        // The size of HeapHeader + 1 address (first block header) will give you the start of the usable memory there
        struct BlockInfo current_block = verifyBlock(starting_place + sizeof(struct HeapHeader) + word_size);
        // The address where the last block ends should always be the last data in general, as such accessing the
        // "start" of the last address + its size should == the maximum space
        current_block.end + word_size == max;
        current_block = verifyBlock(current_block.end + 2*word_size)) {
        if (blockIsInvalid(current_block)) {
            return (struct BoolAndCount){false, current_block.start + word_size};
        }
        if (current_block.flag == inactive) {
            count_free_blocks++;
        }
    }
    return (struct BoolAndCount){true, count_free_blocks};
}

bool verifyFreeList(const int free_list_count) {
    // This is not how we discussed verifying the free list however I implemented the freelist to be non-sequential in
    // terms of address, rather it is based off the last added block. As such I need a different method to verify the free
    // list since walking the blocks won't ensure a sequential walk of the next free block

    // Will return False if the free list is invalid (and true if it is valid)

    int list_cnt = 0;
    struct ListNode last_node = (struct ListNode){0, 0, 0};
    for (struct ListNode current_node = *starting_place.head; current_node.next == 0; current_node = *current_node.next) {
        list_cnt++;
        last_node = current_node;
        if (current_node.header != starting_place) {
            return false;
        }
    }
    if (free_list_count != list_cnt) {
        return false;
    }
    if (last_node.header == 0 && list_cnt == 0) {
        return true;
    }
    if (last_node != *starting_place.tail) {
        return false;
    }
    return true;
}

struct BoolAndCount verify() {
    const struct BoolAndCount heapIntegrityResult = verifyHeapIntegrity();
    if (~heapIntegrityResult.bool_value){return heapIntegrityResult;}
    bool const freeListResult = verifyFreeList(heapIntegrityResult.count_or_address);
    // If FreeList is invalid this will return False and the lack of an address will indicate it's a Free list error
    return (struct BoolAndCount){freeListResult, 0};

}

// Other system func Ideas, get_active_block_count, get_active_block_addresses and sizes

// For get active block addresses there can be any number of active block addresses as such I don't know how I'd be able
// to return that efficiently.

//endregion
