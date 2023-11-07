/** Managing a contiguous region of memory
 *   Contiguous memory allocator:
 *     1. Request for a contiguous block of memory
 *     2. Release of a contiguous block of memory
 *     3. Compact unused holes of memory into one single block
 *     4. Report the regions of free and allocated memory
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#pragma warning(disable : 4996)

#define MAX_MEMORY (4ULL * 1024 * 1024)

// Define a structure for memory blocks
typedef struct MemoryBlock {
	unsigned int start_address;
	size_t size;
	uint8_t is_allocated;
	char process_id[100];
	struct MemoryBlock* next;
} MemoryBlock;

MemoryBlock* memory_head = NULL;
size_t total_memory_size = 0;

void initializeMemory(size_t memory_size) 
{
	total_memory_size = memory_size;
	memory_head = malloc(sizeof(MemoryBlock));
	if (!memory_head) {
		perror("Failed to initialize memory");
		exit(EXIT_FAILURE);
	}
	memory_head->start_address = 0;
	memory_head->size = memory_size;
	memory_head->is_allocated = (uint8_t)0;
	strcpy(memory_head->process_id, "FREE");
	memory_head->next = NULL;
}

void releaseMemory(char* process_id) 
{
	MemoryBlock* current = memory_head;
	MemoryBlock* prev = NULL;

	while (current != NULL) 
	{
		if (current->is_allocated && strcmp(current->process_id, process_id) == 0) 
		{
			current->is_allocated = (uint8_t)0;
			strcpy(current->process_id, "FREE");

			// Check if the previous block is free so we can merge them
			if (prev && !prev->is_allocated) 
			{
				prev->size += current->size;
				prev->next = current->next;
				free(current);
				current = prev;
			}

			// Check if the next block is free so we can merge them
			if (current->next && !current->next->is_allocated) 
			{
				MemoryBlock* next = current->next;
				current->size += next->size;
				current->next = next->next;
				free(next);
			}

			printf("Released memory from process %s\n", process_id);
			return;
		}
		prev = current;
		current = current->next;
	}

	printf("No process with ID %s found.\n", process_id);
}

MemoryBlock* firstFit(size_t size) 
{
	MemoryBlock* current = memory_head;
	while (current != NULL) {
		if (!current->is_allocated && current->size >= size) {
			return current;
		}
		current = current->next;
	}
	return NULL; // No suitable block found
}

MemoryBlock* bestFit(size_t size) 
{
	MemoryBlock* best = NULL;
	MemoryBlock* current = memory_head;

	while (current != NULL) 
	{
		if (!current->is_allocated && current->size >= size &&
			(!best || current->size < best->size)) 
		{
			best = current;
		}
		current = current->next;
	}

	return best;
}

MemoryBlock* worstFit(size_t size) 
{
	MemoryBlock* worst = NULL;
	MemoryBlock* current = memory_head;

	while (current != NULL) 
	{
		if (!current->is_allocated && current->size >= size &&
			(!worst || current->size > worst->size)) 
		{
			worst = current;
		}
		current = current->next;
	}

	return worst;
}

void requestMemory(char* process_id, size_t size, char strategy) 
{
	MemoryBlock* block = NULL;
	switch (strategy) {
	case 'F':
		block = firstFit(size);
		break;
	case 'B':
		block = bestFit(size);
		break;
	case 'W':
		block = worstFit(size);
		break;
	}

	if (block) {
		// Allocate memory by updating the block or splitting it
		if (block->size > size) 
		{
			// Split block
			MemoryBlock* new_block = malloc(sizeof(MemoryBlock));
			if (!new_block) {
				perror("Failed to allocate memory for new block");
				exit(EXIT_FAILURE);
			}
			new_block->start_address = block->start_address + size;
			new_block->size = block->size - size;
			new_block->is_allocated = (uint8_t)0;
			strcpy(new_block->process_id, "FREE");
			new_block->next = block->next;
			block->next = new_block;
			block->size = size;
		}
		block->is_allocated = (uint8_t)1;
		strncpy(block->process_id, process_id, sizeof(block->process_id) - 1);
		printf("Allocated %zu bytes to %s starting at address %u\n", size, process_id, block->start_address);
	}
	else {
		printf("Failed to allocate memory: insufficient space\n");
	}
}

void compactMemory() 
{
	unsigned int next_free_address = 0;
	MemoryBlock* current = memory_head;
	MemoryBlock* last_allocated_block = NULL;

	// Traverse the list and move allocated blocks to the beginning.
	while (current != NULL) {
		if (current->is_allocated) {
			// If the current block is allocated, move it to the next free address.
			current->start_address = next_free_address;
			next_free_address += current->size;
			last_allocated_block = current;
		}
		current = current->next;
	}

	// Now, we should remove all free blocks since they will be merged into one.
	current = memory_head;
	MemoryBlock* prev = NULL;
	while (current != NULL) {
		if (!current->is_allocated) {
			// If the current block is free, remove it from the list.
			MemoryBlock* to_free = current;
			if (prev) {
				prev->next = current->next; // Bypass the current block
			}
			else {
				memory_head = current->next; // Update head if it was the first block
			}
			current = current->next;
			free(to_free);
		}
		else {
			// Update prev if the current block is not free.
			prev = current;
			current = current->next;
		}
	}

	// After moving all allocated blocks to the start, create a single large free block.
	if (next_free_address < total_memory_size) {
		MemoryBlock* free_block = malloc(sizeof(MemoryBlock));
		if (!free_block) {
			perror("Failed to allocate memory for new block");
			exit(EXIT_FAILURE);
		}
		free_block->start_address = next_free_address;
		free_block->size = total_memory_size - next_free_address;
		free_block->is_allocated = 0;
		strcpy(free_block->process_id, "FREE");
		free_block->next = NULL;

		if (last_allocated_block) {
			last_allocated_block->next = free_block;
		}
		else {
			// If there were no allocated blocks, the entire memory is free.
			memory_head = free_block;
		}
	}
	else {
		// If there is no space for a new free block, just make sure the list is terminated.
		if (last_allocated_block) {
			last_allocated_block->next = NULL;
		}
	}

	printf("Memory compaction completed.\n");
}

void reportStatus() 
{
	MemoryBlock* current = memory_head;
	while (current != NULL) {
		printf("Addresses [%u: %u] %s\n",
			current->start_address,
			current->start_address + current->size - 1,
			current->is_allocated ? current->process_id : "Unused");
		current = current->next;
	}
}

void executeCommand(char* command) 
{
	char* token = strtok(command, " \n");
	if (token && strcmp(token, "RQ") == 0) 
	{
		char* process_id = strtok(NULL, " ");
		char* size_str = strtok(NULL, " ");
		char* strategy_str = strtok(NULL, " \n");

		if (process_id && size_str && strategy_str) 
		{
			size_t size = (size_t)atoi(size_str);
			char strategy = strategy_str[0];
			requestMemory(process_id, size, strategy);
		}
		else 
		{
			printf("Invalid RQ command format.\n");
		}
	}
	else if (token && strcmp(token, "RL") == 0) 
	{
		char* process_id = strtok(NULL, " \n");
		if (process_id) 
		{
			releaseMemory(process_id);
		}
		else 
		{
			printf("Invalid RL command format.\n");
		}
	}
	else if (token && strcmp(token, "C") == 0) 
	{
		compactMemory();
	}
	else if (token && strcmp(token, "STAT") == 0) 
	{
		reportStatus();
	}
	else if (token && strcmp(token, "X") == 0) 
	{
		exit(0);
	}
	else 
	{
		printf("Invalid command.\n");
	}
}

int main(int argc, char* argv[])
{
	
	if (argc == 2)
	{
		printf("Contiguous Memory Allocator Project\n");

		char* endptr;
		size_t memory_size = (size_t)strtoul(argv[1], &endptr, 10);

		// Check for various possible errors
		if (endptr == argv[1]) 
		{
			fprintf(stderr, "No digits were found\n");
			return EXIT_FAILURE;
		}
		if (*endptr != '\0') 
		{
			fprintf(stderr, "Further characters after number: %s\n", endptr);
			return EXIT_FAILURE;
		}
		if (memory_size == 0 || memory_size > MAX_MEMORY) 
		{
			fprintf(stderr, "Invalid memory size. Must be > 0 and <= %llu\n", (unsigned long long)MAX_MEMORY);
			return EXIT_FAILURE;
		}

		initializeMemory(memory_size);

		char command[256];
		while (1)
		{
			printf("allocator> ");
			fflush(stdout); // Make sure "allocator>" is printed
			if (!fgets(command, sizeof(command), stdin)) {
				continue;
			}

			executeCommand(command);
		}
	}
	else
	{
		printf("Usage: %s <memory size>\n", argv[0]);
	}

	
	return 0;
}
