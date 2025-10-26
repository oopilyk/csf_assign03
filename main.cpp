#include <iostream>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <algorithm>

struct CacheBlock {
    bool valid;
    bool dirty;
    unsigned int tag;
    unsigned int lru_count;
    
    CacheBlock() : valid(false), dirty(false), tag(0), lru_count(0) {}
};

struct CacheSet {
    std::vector<CacheBlock> blocks;
    
    CacheSet(int associativity) : blocks(associativity) {}
};

class CacheSimulator {
private:
    int num_sets;
    int num_blocks_per_set;
    int block_size;
    bool write_allocate;
    bool write_through;
    bool lru_eviction;
    
    std::vector<CacheSet> sets;
    unsigned int global_counter;
    
    // stats for the cache
    int total_loads;
    int total_stores;
    int load_hits;
    int load_misses;
    int store_hits;
    int store_misses;
    int total_cycles;
    
    int set_bits;
    int block_bits;
    int tag_bits;
    
public:
    CacheSimulator(int sets, int blocks_per_set, int bytes_per_block, 
                   bool write_alloc, bool write_thru, bool lru) 
        : num_sets(sets), num_blocks_per_set(blocks_per_set), block_size(bytes_per_block),
          write_allocate(write_alloc), write_through(write_thru), lru_eviction(lru),
          sets(sets, CacheSet(blocks_per_set)), global_counter(0),
          total_loads(0), total_stores(0), load_hits(0), load_misses(0),
          store_hits(0), store_misses(0), total_cycles(0) {
        
        // bit possitioning
        set_bits = std::log2(num_sets);
        block_bits = std::log2(block_size);
        tag_bits = 32 - set_bits - block_bits;
    }
    
    void processAccess(char operation, unsigned int address) {
        unsigned int set_index = (address >> block_bits) & ((1 << set_bits) - 1);
        unsigned int tag = address >> (set_bits + block_bits);
        
        if (operation == 'l') {
            processLoad(set_index, tag);
        } else {
            processStore(set_index, tag);
        }
    }
    
private:
    void processLoad(unsigned int set_index, unsigned int tag) {
        total_loads++;
        total_cycles++;
        CacheSet& set = sets[set_index];
        
        // if hit then increase the hit stat
        for (int i = 0; i < num_blocks_per_set; i++) {
            if (set.blocks[i].valid && set.blocks[i].tag == tag) {
                load_hits++;
                set.blocks[i].lru_count = ++global_counter;
                return;
            }
        }
        // it was a miss
        load_misses++;
        total_cycles += 100 * (block_size / 4);
        
        allocateBlock(set, tag);
    }
    
    void processStore(unsigned int set_index, unsigned int tag) {
        total_stores++;
        total_cycles++;
        CacheSet& set = sets[set_index];
        
        for (int i = 0; i < num_blocks_per_set; i++) {
            if (set.blocks[i].valid && set.blocks[i].tag == tag) {
                store_hits++;
                set.blocks[i].lru_count = ++global_counter;
                if (!write_through) {
                    set.blocks[i].dirty = true;
                } else {
                    total_cycles += 100;
                }
                return;
            }
        }
        store_misses++;
        
        if (write_allocate) {
            total_cycles += 100 * (block_size / 4);
            allocateBlock(set, tag);
            if (!write_through) {
                for (int i = 0; i < num_blocks_per_set; i++) {
                    if (set.blocks[i].valid && set.blocks[i].tag == tag) {
                        set.blocks[i].dirty = true;
                        break;
                    }
                }
            } else {
                total_cycles += 100;
            }
        } else {
            total_cycles += 100;
        }
    }
    
    void allocateBlock(CacheSet& set, unsigned int tag) {
        for (int i = 0; i < num_blocks_per_set; i++) {
            if (!set.blocks[i].valid) {
                set.blocks[i].valid = true;
                set.blocks[i].tag = tag;
                set.blocks[i].lru_count = ++global_counter;
                set.blocks[i].dirty = false;
                return;
            }
        }
        evictBlock(set, tag);
    }
    // make room for new blocks
    void evictBlock(CacheSet& set, unsigned int tag) {
        int evict_index = 0;
        
        if (lru_eviction) {
            unsigned int min_counter = set.blocks[0].lru_count;
            for (int i = 1; i < num_blocks_per_set; i++) {
                if (set.blocks[i].lru_count < min_counter) {
                    min_counter = set.blocks[i].lru_count;
                    evict_index = i;
                }
            }
        } else {
            // fifo doesnt work yet
            unsigned int min_counter = set.blocks[0].lru_count;
            for (int i = 1; i < num_blocks_per_set; i++) {
                if (set.blocks[i].lru_count < min_counter) {
                    min_counter = set.blocks[i].lru_count;
                    evict_index = i;
                }
            }
        }
    
        if (set.blocks[evict_index].dirty && !write_through) {
            total_cycles += 100 * (block_size / 4); // Writeback to memory
        }

        set.blocks[evict_index].valid = true;
        set.blocks[evict_index].tag = tag;
        set.blocks[evict_index].lru_count = ++global_counter;
        set.blocks[evict_index].dirty = false;
    }
    
public:
    void printStats() {
        std::cout << "Total loads: " << total_loads << std::endl;
        std::cout << "Total stores: " << total_stores << std::endl;
        std::cout << "Load hits: " << load_hits << std::endl;
        std::cout << "Load misses: " << load_misses << std::endl;
        std::cout << "Store hits: " << store_hits << std::endl;
        std::cout << "Store misses: " << store_misses << std::endl;
        std::cout << "Total cycles: " << total_cycles << std::endl;
    }
};

int main( int argc, char **argv ) {
  if (argc != 7) {
    
    std::cerr << "Incorect number of arguments. Should be: "<<
                            "\n - number of sets in the cache (a positive power-of-2)"<<
                            "\n - number of blocks in each set (a positive power-of-2)"<<
                            "\n - number of bytes in each block (a positive power-of-2, at least 4)"<<
                            "\n - write-allocate or no-write-allocate"<<
                            "\n - write-through or write-back"<<
                            "\n - lru (least-recently-used) or fifo evictions" << std::endl;
    return 1;

  }

  if (std::floor(std::log2(std::atoi(argv[1]))) != std::log2(std::atoi(argv[1]))) {

    std::cerr << "number of sets in cache must be a power of 2" << std::endl;
    return 1;

  }

  if (std::floor(std::log2(std::atoi(argv[2]))) != std::log2(std::atoi(argv[2]))) {

    std::cerr << "number of blocks in each set must a be power of 2" << std::endl;
    return 1;
    
  }

  if (std::floor(std::log2(std::atoi(argv[3]))) != std::log2(std::atoi(argv[3])) || std::atoi(argv[3]) < 4) {

    std::cerr << "number of bytes in each block must be a positive power-of-2, at least 4" << std::endl;
    return 1;
    
  }

  if (strcmp(argv[4], "write-allocate") != 0 && strcmp(argv[4], "no-write-allocate") != 0) {

    std::cerr << "cache miss parameter must be write-allocate or no-write-allocate" << std::endl;
    return 1;
    
  }

  if (strcmp(argv[5], "write-through") != 0 && strcmp(argv[5], "write-back") != 0) {

    std::cerr << "store write parameter must be write-through or write-back" << std::endl;
    return 1;
    
  }

  if (strcmp(argv[6], "lru") != 0 && strcmp(argv[6], "fifo") != 0) {

    std::cerr << "eviction parameter must be lru of fifo" << std::endl;
    return 1;
    
  }

  // Parse arguments
  int num_sets = std::atoi(argv[1]);
  int num_blocks = std::atoi(argv[2]);
  int block_size = std::atoi(argv[3]);
  bool write_allocate = (strcmp(argv[4], "write-allocate") == 0);
  bool write_through = (strcmp(argv[5], "write-through") == 0);
  bool lru = (strcmp(argv[6], "lru") == 0);
  
  // Check for invalid combination
  if (!write_allocate && !write_through) {

    std::cerr << "no-write-allocate and write-back is an invalid combination" << std::endl;
    return 1;

  }
  
  // Create cache simulator
  CacheSimulator cache(num_sets, num_blocks, block_size, write_allocate, write_through, lru);
  
  // Read trace from stdin
  char operation;
  std::string address_str;
  int size;
  
  while (std::cin >> operation >> address_str >> size) {
    // Parse hexadecimal address
    unsigned int address = std::stoul(address_str, nullptr, 16);
    cache.processAccess(operation, address);
  }
  
  // Print statistics
  cache.printStats();
  
  return 0;
}
