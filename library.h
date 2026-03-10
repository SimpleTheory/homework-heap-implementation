#ifndef HEAP_LIBRARY_H
#define HEAP_LIBRARY_H
#include <stdbool.h>

// TODO Write doc comment on each exposed func for api (promises(exactly, what you get, expectations), constraints, examples)
int allocate(int size);
bool free_address(int *address);

#endif // HEAP_LIBRARY_H