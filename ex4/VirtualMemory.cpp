#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>




/**

@brief Helper function to calculate the cyclical distance between two addresses.
@param currAddress The current address.
@param virtualAddress The virtual address.
@return The cyclical distance between the two addresses.
*/

uint64_t CalculateDistance(uint64_t currAddress, uint64_t virtualAddress) {
    uint64_t abs = (virtualAddress > currAddress) ?
                   (virtualAddress - currAddress) : (currAddress - virtualAddress);
    return abs;
}


/**

@brief Helper function to calculate the eviction distance between two addresses.
@param currAddress The current address.
@param virtualAddress The virtual address.
@return The eviction distance between the two addresses.
*/

uint64_t CalculateEvictionDistance(uint64_t currAddress, uint64_t virtualAddress) {
    uint64_t distance = CalculateDistance(currAddress, virtualAddress);
    uint64_t diff = NUM_PAGES - distance;
    return (diff < distance) ? diff : distance;
}

/**
 * Update the maximum value in the table.
 *
 * @param maxTable Pointer to the maximum value in the table.
 * @param currentBaby The current value.
 */

void UpdateMaxTable(word_t *maxTable, word_t currentBaby) {

  if (currentBaby > *maxTable)
  {
	  *maxTable = currentBaby;
  }

}



/**
 * Recursive function to traverse the page table tree and find the farthest node.
 *
 * @param maxTable Pointer to the maximum value in the table.
 * @param papaAddress The parent address.
 * @param currLevelOfDepth The current level of depth.
 * @param current The current node.
 * @param parent The parent node.
 * @param farthestNode Pointer to the farthest node.
 * @param farthestNodeParent Pointer to the parent of the farthest node.
 * @param virtualAddress The virtual address.
 * @param currAddress The current address.
 * @param farthestNodeVA Pointer to the address of the farthest node.
 * @param furthest Pointer to the furthest distance.
 * @return The new table created during traversal, if any.
 */

word_t DfsTreeSearchHelper(word_t *maxTable, word_t papaAddress, uint64_t currLevelOfDepth, word_t current,
                        uint64_t parent, word_t *farthestNode, uint64_t *farthestNodeParent,
                        uint64_t virtualAddress, uint64_t currAddress,
                        uint64_t *farthestNodeVA, uint64_t *furthest) {
    // Stopping the recursive call if we reach the leaf nodes
    if (currLevelOfDepth == TABLES_DEPTH) {
        uint64_t endDistance = CalculateEvictionDistance(currAddress, virtualAddress);

        if (endDistance > *furthest) {
            *furthest = endDistance;
            *farthestNodeVA = currAddress;
            *farthestNode = current;
            *farthestNodeParent = parent;
        }

        return 0;
    }

    word_t newTable = 0;
    bool nullLine = true;
    int i = 0;

    while (i < PAGE_SIZE) {
        word_t currentBaby = 0;
        PMread(current * PAGE_SIZE + i, &currentBaby);

        UpdateMaxTable(maxTable, currentBaby);

        if (currentBaby) {
            newTable = DfsTreeSearchHelper(maxTable, papaAddress, currLevelOfDepth + 1, currentBaby,
                                        current * PAGE_SIZE + i, farthestNode, farthestNodeParent,
                                        virtualAddress, (currAddress << OFFSET_WIDTH) + i,
                                        farthestNodeVA, furthest);
            nullLine = false;
            if (newTable) {
                return newTable;
            }
        }

        i++;
    }

    if (!nullLine || current == papaAddress) {
        // Do nothing
    } else {
        PMwrite(parent, 0);
        return current;
    }

    return 0;
}


/**
 * Wrapper function for the recursive tree traversal.
 *
 * @param maxTable Pointer to the maximum value in the table.
 * @param papaAddress The parent address.
 * @param currLevelOfDepth The current level of depth.
 * @param current The current node.
 * @param parent The parent node.
 * @param farthestNode Pointer to the farthest node.
 * @param farthestNodeParent Pointer to the parent of the farthest node.
 * @param virtualAddress The virtual address.
 * @param currAddress The current address.
 * @param farthestNodeVA Pointer to the address of the farthest node.
 * @param furthest Pointer to the furthest distance.
 * @return The new table created during traversal, if any.
 */
word_t DfsTreeSearch(word_t *maxTable, word_t papaAddress, uint64_t currLevelOfDepth, word_t current,
                  uint64_t parent, word_t *farthestNode, uint64_t *farthestNodeParent,
                  uint64_t virtualAddress, uint64_t currAddress,
                  uint64_t *farthestNodeVA, uint64_t *furthest) {
    return DfsTreeSearchHelper(maxTable, papaAddress, currLevelOfDepth, current, parent,
                            farthestNode, farthestNodeParent, virtualAddress,
                            currAddress, farthestNodeVA, furthest);
}



/**
 * Determine whether to evict a page or return a new table.
 *
 * @param newTable The new table created during traversal.
 * @param maxTable The maximum value in the table.
 * @param farthestNode The farthest node.
 * @param farthestNodeParent The parent of the farthest node.
 * @param farthestNodeVA The address of the farthest node.
 * @return The new table or the evicted page.
 */

word_t EvictOrReturnTable(word_t newTable, word_t maxTable, word_t farthestNode,
                          uint64_t farthestNodeParent, uint64_t farthestNodeVA) {
    if (newTable != 0) {
        return newTable;
    }

    if (maxTable + 1 < NUM_FRAMES) {
        return (maxTable + 1);
    }

    PMevict(farthestNode, farthestNodeVA); // Evicting the chosen Table to the disk.
    PMwrite(farthestNodeParent, 0); // Writing to the father of the chosen Table 0.
    return farthestNode;
}



/**
 * Bring the page table for the given parent address and virtual address.
 *
 * @param papaAddress The parent address.
 * @param virtualAddress The virtual address.
 * @return The new table or the evicted page.
 */

word_t bringTable(word_t papaAddress, uint64_t virtualAddress) {
    word_t maxTable = 0;
    word_t farthestNode = 0;
    uint64_t farthestNodeParent = 0;
    uint64_t farthestNodeVA = 0;
    uint64_t furthest = 0;
    word_t newTable = DfsTreeSearch(&maxTable, papaAddress, 0, 0, 0, &farthestNode, &farthestNodeParent,
                                 virtualAddress, 0, &farthestNodeVA, &furthest);
    return EvictOrReturnTable(newTable, maxTable, farthestNode, farthestNodeParent,
                              farthestNodeVA);
}

/**
 * Initialize the table by setting all values to 0.
 *
 * @param table The table to initialize.
 */

void initializeTable(uint64_t table) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(table * PAGE_SIZE + i, 0);
    }
}


/**
 * Split the address into offset and remaining address parts.
 *
 * @param initialAddress The initial address.
 * @param offset The offset.
 * @param resAddress The remaining address.
 */
void SplitAddress(uint64_t initialAddress, uint64_t& offset, uint64_t& resAddress) {
    offset = initialAddress & ((1 << OFFSET_WIDTH) - 1);
    resAddress = initialAddress >> OFFSET_WIDTH;
}


/**
 * Save the address for each depth of the tables in an array.
 *
 * @param resAddress The remaining address.
 * @param addressList The array to store the addresses.
 */
void SaveAddressLists(uint64_t resAddress, uint64_t* addressList) {
    for (int i = TABLES_DEPTH - 1; i >= 0; --i) {
        addressList[i] = resAddress & (PAGE_SIZE - 1);
        resAddress >>= OFFSET_WIDTH;
    }
}


/**
 * Find the physical address for the given virtual address.
 *
 * @param addressList The array of addresses.
 * @param currBaseAddress The current base address.
 * @param initialAddress The initial virtual address.
 * @return The physical address.
 */
uint64_t FindPhysicalAddress(const uint64_t* addressList, word_t& currBaseAddress, uint64_t initialAddress) {
    uint64_t offset = initialAddress & ((1 << OFFSET_WIDTH) - 1);
    int i = 0;
    while (i < TABLES_DEPTH) {
        word_t papaAddress = currBaseAddress;
        PMread(currBaseAddress * PAGE_SIZE + addressList[i], &currBaseAddress);
        if (!currBaseAddress) {
            word_t newTable = bringTable(papaAddress, initialAddress >> OFFSET_WIDTH);
            if (i == TABLES_DEPTH - 1) {
                PMrestore(newTable, initialAddress >> OFFSET_WIDTH);
            } else {
                initializeTable(newTable);
            }
            PMwrite(papaAddress * PAGE_SIZE + addressList[i], newTable);
            currBaseAddress = newTable;
        }
        i++;
    }
    return currBaseAddress * PAGE_SIZE + offset;
}



/**
 * Convert the virtual address to the physical address.
 *
 * @param initialAddress The initial virtual address.
 * @return The physical address.
 */

uint64_t addressBringer(uint64_t initialAddress) {
    uint64_t offset, resAddress;
    SplitAddress(initialAddress, offset, resAddress);

    uint64_t addressList[TABLES_DEPTH];
    SaveAddressLists(resAddress, addressList);

    word_t currBaseAddress = 0;
    return FindPhysicalAddress(addressList, currBaseAddress, initialAddress);
}





/**
 * Initialize the virtual memory by setting all values to 0.
 */
void VMinitialize() {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(i, 0);
    }
}


/**
 * Read the value from the virtual memory at the specified virtual address.
 *
 * @param virtualAddress The virtual address to read from.
 * @param value Pointer to store the read value.
 * @return 1 if successful, 0 otherwise.
 */

int VMread(uint64_t virtualAddress, word_t *value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) return 0;
    PMread(addressBringer(virtualAddress), value);
    return 1;
}


/**
 * Write the value to the virtual memory at the specified virtual address.
 *
 * @param virtualAddress The virtual address to write to.
 * @param value The value to write.
 * @return 1 if successful, 0 otherwise.
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) return 0;
    PMwrite(addressBringer(virtualAddress), value);
    return 1;
}






