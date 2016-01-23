#ifndef __CHRONOS_INTERNAL_H__
#define __CHRONOS_INTERNAL_H__

void make_platform_agnostic( struct index_entry * entry, struct index_entry * out_platform_agnostic );
void make_platform_specific( struct index_entry * out_entry, struct index_entry * platform_agnostic );

enum chronos_file {
	cf_index,
	cf_data_store,
};
// open the file specified if that hasn't already been done.
// if we are required to change from read to read-write then the file lock is updated.
int require_open_file( struct chronos_handle * handle, enum chronos_file file, enum chronos_flags flag );

// sets the time in the key
int init_key( struct index_key * out_key );

#endif /* __CHRONOS_INTERNAL_H__ */
