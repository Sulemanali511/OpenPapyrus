/* This is a public domain general purpose hash table package written by Peter Moore @ UCB. */

/* @(#) st.h 5.1 89/12/14 */

#ifndef ST_INCLUDED
#define ST_INCLUDED

#if SIZEOF_VOIDP == SIZEOF_LONG
	typedef unsigned long st_data_t;
#elif SIZEOF_VOIDP == SIZEOF_LONG_LONG
	typedef unsigned long long st_data_t;
#endif
#define ST_DATA_T_DEFINED

struct StStrEndKey;

struct st_hash_type {
    int (*compare)(void *, void *);
    int (*hash)(void *);
};

struct st_table {
    struct st_hash_type *type;
    int num_bins;
    int num_entries;
    struct st_table_entry **bins;
};

#define st_is_member(table,key) st_lookup(table,key,(st_data_t *)0)

enum st_retval {ST_CONTINUE, ST_STOP, ST_DELETE, ST_CHECK};

#ifndef _
	#define _(args) args
#endif
#ifndef ANYARGS
	#ifdef __cplusplus
		#define ANYARGS ...
	#else
		#define ANYARGS
	#endif
#endif

st_table * st_init_table(struct st_hash_type *);
st_table * st_init_table_with_size(struct st_hash_type *, int);
st_table * st_init_numtable(void);
st_table * st_init_numtable_with_size(int);
st_table * st_init_strtable(void);
st_table * st_init_strtable_with_size(int);
int st_delete(st_table *, st_data_t *, st_data_t *);
int st_delete_safe(st_table *, st_data_t *, st_data_t *, st_data_t);
int st_insert(st_table *, st_data_t, st_data_t);
int st_lookup(st_table *, st_data_t, st_data_t *);
int st_foreach(st_table *, int (*)(ANYARGS), st_data_t);
void st_add_direct(st_table *, st_data_t, st_data_t);
void st_free_table(st_table *);
void st_cleanup_safe(st_table *, st_data_t);
st_table * st_copy(st_table *);

#define ST_NUMCMP	((int (*)()) 0)
#define ST_NUMHASH	((int (*)()) -2)

#define st_numcmp	ST_NUMCMP
#define st_numhash	ST_NUMHASH

#endif /* ST_INCLUDED */
