#ifndef MEMORY_H
#define MEMORY_H

#include "global.h"
#include "bus.h"

typedef struct {
    uint32_t data[MAIN_MEMORY_SIZE];
} MainMemory;

void memory_init(MainMemory *mem);

// Memory behaves as a slave to the bus. 
// It sees a BusRd/BusRdX and responds with Flush after latency.
void memory_listen(MainMemory *mem, Bus *bus);

#endif // MEMORY_H