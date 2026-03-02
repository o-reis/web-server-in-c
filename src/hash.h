#ifndef HASH_H
#define HASH_H

typedef struct hash_elem_t {    
	struct hash_elem_t* next;
	void* data;
	char key[];
} hash_elem_t;

typedef struct {
	unsigned int capacity;
	unsigned int e_num;
	hash_elem_t** table;
} hashtable_t;

hashtable_t* ht_create(unsigned int capacity);
void* ht_put(hashtable_t* hasht, char* key, void* data);
void* ht_get(hashtable_t* hasht, char* key);
void* ht_remove(hashtable_t* hasht, char* key);
void ht_clear(hashtable_t* hasht, int free_data);
void ht_destroy(hashtable_t* hasht, int free_data);




#endif