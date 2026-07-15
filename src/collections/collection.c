// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdbool.h>
// #include "collections/collection.h"
// #include "memory/cmemory.h"

// //----------------------------------------------------------------------------------
// // Global Variables Definition (local to this module)
// // void AdjustFrontRear(Collection *c);

// // Create a new queue with specified element size and count
// Collection *NewCollection(int elemCount, size_t elemSize)
// {
//     Collection *c = AllocateBytes(sizeof(Collection));
//     c->elemSize = elemSize;
//     c->capacity = elemCount;
//     c->count = 0;
//     c->front = 0; // Front points to the next item to be popped
//     c->rear = 0;  // Rear points to the next empty slot where data will be written
//     c->enumeratorIndex = 0;
//     c->enumerationCount = 0;
//     c->items = AllocateCollection(elemCount, elemSize);

//     // Simple safety check
//     if (c->items == NULL)
//     {
//         fprintf(stderr, "Failed to allocate memory for Dynamic Array!\n");
//         c->capacity = 0; // Ensure nothing can be pushed
//     }
//     return c;
// }

// // Enumerate through the collection, returning a pointer to each item until we hit rear (the next empty slot), at which point we reset the enumerator and return NULL to signal the end of enumeration.
// void *Enumerate(Collection *c)
// {
//     // Rear points to the next empty slot where data will be written, so we want to stop enumerating once we hit rear, not capacity
//     if (c == NULL || c->count == 0)
//         return NULL;

//     // Reset if we've enumerated all items (safety check using enumerationCount to prevent infinite loops in case of bugs)
//     if (c->enumerationCount == c->count)
//     {
//         ResetEnumerator(c); // Reset enumerator for the next time we want to enumerate
//         return NULL;
//     }

//     // Calculate the address of the current enumerator index
//     void *item = (char *)c->items + (c->enumeratorIndex * c->elemSize);

//     // Move the enumerator index to the next item for the next call and increment the enumeration count
//     c->enumeratorIndex = (c->enumeratorIndex + 1) % c->capacity;
//     c->enumerationCount++;

//     return item;
// }

// void *ResetEnumerator(Collection *c)
// {
//     // Reset enumerator to the front
//     if (c != NULL)
//     {
//         c->enumeratorIndex = c->front;
//         c->enumerationCount = 0; // Reset the progress tracker!
//     }
// }

// // Expand the collection's capacity by doubling it, and copying existing items to the new memory block. This is a common strategy for dynamic arrays to maintain amortized O(1) time complexity for push operations.
// bool GrowCollection(Collection *c)
// {
//     if (c == NULL)
//         return false;

//     if (c->elemSize < 1)
//     {
//         fprintf(stderr, "Invalid element size %zu in GrowCollection! Must be greater than 0.\n", c->elemSize);
//         return false;
//     }

//     int newCapacity = (c->capacity == 0) ? 4 : c->capacity * 2;
//     // Note: We use malloc here because we are manually re-ordering,
//     // so we don't need realloc to copy the "old" scrambled order.
//     void *newItems = AllocateCollection(newCapacity, c->elemSize);

//     if (newItems == NULL)
//     {
//         fprintf(stderr, "Failed to allocate memory for growing collection to new capacity %d!\n", newCapacity);
//         return false;
//     }

//     if (c->count > 0)
//     {
//         // 1. Copy from 'front' to the physical end of the old buffer
//         int firstPartCount = c->capacity - c->front;
//         memcpy(newItems, (char *)c->items + (c->front * c->elemSize), firstPartCount * c->elemSize);

//         // 2. Copy the wrapped part (from 0 to rear) to immediately after the first part
//         if (c->front > 0) // Only need this if we actually wrapped
//         {
//             memcpy((char *)newItems + (firstPartCount * c->elemSize), c->items, c->rear * c->elemSize);
//         }
//     }

//     free(c->items); // Get rid of the old, scrambled buffer

//     c->items = newItems;
//     c->capacity = newCapacity;
//     c->front = 0;       // Start is now at the beginning
//     c->rear = c->count; // Next empty slot is at the end of the data

//     return true;
// }

// // Get the current number of elements in the collection
// size_t GetElementCount(Collection *c)
// {
//     if (c == NULL)
//         return 0;
//     return c->count;
// }

// // Dispose of the collection and free its memory
// void DisposeCollection(Collection *c)
// {
//     if (c == NULL)
//         return;
//     if (c->items != NULL)
//     {
//         DeallocateShallow(&c->items, c->capacity * c->elemSize);
//         c->items = NULL;
//     }
//     c->count = 0;
//     c->capacity = 0;
// }
