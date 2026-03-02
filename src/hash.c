#include <stdlib.h>
#include <string.h>
#include "hash.h"

char err_ptr;
void *HT_ERROR = &err_ptr;

// Calculates a hash value for a given string using djb2 algorithm
static unsigned int ht_calc_hash(char *str)
{
	unsigned int hash = 5381;
	int c;
	while ((c = *str++) != 0)
		hash = ((hash << 5) + hash) + c;

	return hash;
}

// Creates a new hash table with the specified capacity
hashtable_t *ht_create(unsigned int capacity)
{
	hashtable_t *hasht = (hashtable_t *)malloc(sizeof(hashtable_t));
	if (!hasht)
		return NULL;
	if ((hasht->table = (hash_elem_t **)malloc(capacity * sizeof(hash_elem_t *))) == NULL)
	{
		free(hasht);
		return NULL;
	}
	hasht->capacity = capacity;
	hasht->e_num = 0;
	unsigned int i;
	for (i = 0; i < capacity; i++)
		hasht->table[i] = NULL;
	return hasht;
}

// Inserts or updates a key-value pair in the hash table
void *ht_put(hashtable_t *hasht, char *key, void *data)
{
	if (data == NULL)
		return NULL;
	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t *e = hasht->table[h];

	while (e != NULL)
	{
		if (!strcmp(e->key, key))
		{
			void *ret = e->data;
			e->data = data;
			return ret;
		}
		e = e->next;
	}

	if ((e = (hash_elem_t *)malloc(sizeof(hash_elem_t) + strlen(key) + 1)) == NULL)
		return HT_ERROR;
	strcpy(e->key, key);
	e->data = data;

	e->next = hasht->table[h];
	hasht->table[h] = e;
	hasht->e_num++;

	return NULL;
}

// Retrieves the value associated with a key from the hash table
void *ht_get(hashtable_t *hasht, char *key)
{
	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t *e = hasht->table[h];
	while (e != NULL)
	{
		if (!strcmp(e->key, key))
			return e->data;
		e = e->next;
	}
	return NULL;
}

// Removes a key-value pair from the hash table and returns the value
void *ht_remove(hashtable_t *hasht, char *key)
{
	unsigned int h = ht_calc_hash(key) % hasht->capacity;
	hash_elem_t *e = hasht->table[h];
	hash_elem_t *prev = NULL;
	while (e != NULL)
	{
		if (!strcmp(e->key, key))
		{
			void *ret = e->data;
			if (prev != NULL)
				prev->next = e->next;
			else
				hasht->table[h] = e->next;
			free(e);
			e = NULL;
			hasht->e_num--;
			return ret;
		}
		prev = e;
		e = e->next;
	}
	return NULL;
}

// Clears all entries from the hash table, optionally freeing data
void ht_clear(hashtable_t *hasht, int free_data)
{
	size_t i;
	for (i = 0; i < hasht->capacity; i++)
	{
		hash_elem_t *e = hasht->table[i];
		while (e != NULL)
		{
			hash_elem_t *next = e->next;

			if (free_data && e->data != NULL)
				free(e->data);

			free(e);

			e = next;
		}
		hasht->table[i] = NULL;
	}
	hasht->e_num = 0;
}

// Destroys the hash table and frees all memory
void ht_destroy(hashtable_t *hasht, int free_data)
{
	ht_clear(hasht, free_data);
	free(hasht->table);
	free(hasht);
}