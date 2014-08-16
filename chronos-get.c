#include "chronos.h"

#include "headers.h"

// for mmap stuff
#include <sys/mman.h>

int compare( struct index_key * key1, struct index_key * key2 ){
	if( key1->seconds < key2->seconds ){
		return -1;
	}
	if( key1->seconds > key2->seconds ){
		return 1;
	}
	if( key1->nanos < key2->nanos ){
		return -1;
	}
	if( key1->nanos > key2->nanos ){
		return 1;
	}
	return 0;
}

int find2( struct index_entry * entries, struct index_key * key, int start, int end, int count ){
	int mid = (end - start) / 2 + start;

	struct index_key * lookup = & entries[mid].key;

	int cmp = compare( key, lookup );
	if( cmp == 0 ){
		return mid;
	}

	// we ran out of search space, but didn't find the key
	if( mid == start || mid == end ){
		return -1;
	}

	if( cmp < 0 ){
		return find2( entries, key, start, mid, count );
	} else if( 0 < cmp ){
		return find2( entries, key, mid, end, count );
	}

	return -2;
}

int find( struct index_entry * entries, struct index_key * key, int count ){
	return find2( entries, key, 0, count, count );
}

int get( int argc, char** argv ){
	char * dir = argv[2];

	if( argc != 4 ){
		fputs("wrong number of arguments to get call\n", stdout);
		return -1;
	}

	// parse this?
	struct index_key key;

	int rc = parse_key( argv[3], strlen(argv[3]), &key );
	if( rc != 0 ){
		printf("failed to parse key: '%s'", argv[3] );
		return -1;
	}
	//printf("key: { .seconds = %d, .nanos = %d }", key.seconds, key.nanos);

	int fd_dir = lock_dir( dir, 0 );

	if( fd_dir == -1 ){
		return -1;
	}

	int entry_count = 0;
	int fd_index = open_index( dir, 0, &entry_count );

	if( fd_index == -1 ){
		return -1;
	}

	void* map = mmap( NULL // memory offset (only a suggestion)
			, entry_count * sizeof(struct index_entry) // size of area
			, PROT_READ // special stuff, read only memory
			, MAP_PRIVATE // mode
			, fd_index // file descriptor
			, 0 ); // file content offset

	if( map == (void*)-1 ){
		fputs( strerror( errno ), stderr );
		fputs("\nmmap( * ) failed\n", stderr);
		return -1;
	}

	// binary search.
	int index = find( (struct index_entry*) map, & key, entry_count );

	struct index_entry entry;
	if( index >= 0 ){
		entry = ((struct index_entry*)map)[ index ];
	} else {
		printf("couldn't find key: %d\n", index);
	}

	// un-mmap
	munmap( map, entry_count * sizeof(struct index_entry) );

	close( fd_index );

	if( index != -1 ){

		int fd_data = open_data( dir, 0 );

		if( fd_data == -1 ){
			return -1;
		}

		lseek( fd_data, entry.position, SEEK_SET );

		int rc = write_out( fd_data, 1, entry.length );
		if( rc == -1 ){
			fputs("failed to write data out\n", stderr);
			return -1;
		}

		close( fd_data );
	}

	release_dir(fd_dir);

	if( index == -1 ){
		return -1;
	}

	return 0;
}
