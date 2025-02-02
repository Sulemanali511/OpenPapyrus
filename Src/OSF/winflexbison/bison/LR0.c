/* Generate the LR(0) parser states for Bison.

   Copyright (C) 1984, 1986, 1989, 2000-2002, 2004-2015, 2018-2020 Free
   Software Foundation, Inc.

   This file is part of Bison, the GNU Compiler Compiler.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* See comments in state.h for the data structures that represent it.
   The entry point is generate_states.  */

#include "bison.h"
#pragma hdrstop

typedef struct state_list {
	struct state_list * next;
	state * state;
} state_list;

static state_list * first_state = NULL; // @global
static state_list * last_state = NULL;  // @global

/* Print CORE for debugging. */
static void core_print(size_t core_size, item_index * core, FILE * out)
{
	for(size_t i = 0; i < core_size; ++i) {
		item_print(ritem + core[i], NULL, out);
		fputc('\n', out);
	}
}

/*-----------------------------------------------------------------.
| A state was just discovered by transitioning on SYM from another |
| state.  Queue this state for later examination, in order to find |
| its outgoing transitions.  Return it.                            |
   `-----------------------------------------------------------------*/

static state * state_list_append(symbol_number sym, size_t core_size, item_index * core)
{
	state_list * node = (state_list *)xmalloc(sizeof *node);
	state * res = state_new(sym, core_size, core);
	if(trace_flag & trace_automaton)
		fprintf(stderr, "state_list_append (state = %d, symbol = %d (%s))\n", nstates, sym, symbols[sym]->tag);
	node->next = NULL;
	node->state = res;
	if(!first_state)
		first_state = node;
	if(last_state)
		last_state->next = node;
	last_state = node;
	return res;
}

/* Symbols that can be "shifted" (including nonterminals) from the current state.  */
bitset shift_symbol; // @global

static rule ** redset; // @global
/* For the current state, the list of pointers to states that can be
   reached via a shift/goto.  Could be indexed by the reaching symbol,
   but labels of incoming transitions can be recovered by the state
   itself.  */
static state ** shiftset; // @global

/* KERNEL_BASE[symbol-number] -> list of item indices (offsets inside
   RITEM) of length KERNEL_SIZE[symbol-number]. */
static item_index ** kernel_base; // @global
static int * kernel_size; // @global

/* A single dimension array that serves as storage for KERNEL_BASE.  */
static item_index * kernel_items; // @global

static void allocate_itemsets(void)
{
	/* Count the number of occurrences of all the symbols in RITEMS.
	   Note that useless productions (hence useless nonterminals) are
	   browsed too, hence we need to allocate room for _all_ the
	   symbols.  */
	size_t count = 0;
	size_t * symbol_count = (size_t *)xcalloc(nsyms + nuseless_nonterminals, sizeof *symbol_count);
	for(rule_number r = 0; r < nrules; ++r)
		for(item_number * rhsp = rules[r].rhs; 0 <= *rhsp; ++rhsp) {
			symbol_number sym = item_number_as_symbol_number(*rhsp);
			count += 1;
			symbol_count[sym] += 1;
		}

	/* See comments before new_itemsets.  All the vectors of items
	   live inside KERNEL_ITEMS.  The number of active items after
	   some symbol S cannot be more than the number of times that S
	   appears as an item, which is SYMBOL_COUNT[S].
	   We allocate that much space for each symbol.  */

	kernel_base = (item_index **)xnmalloc(nsyms, sizeof *kernel_base);
	kernel_items = (item_index *)xnmalloc(count, sizeof *kernel_items);

	count = 0;
	for(symbol_number i = 0; i < nsyms; i++) {
		kernel_base[i] = kernel_items + count;
		count += symbol_count[i];
	}
	SAlloc::F(symbol_count);
	kernel_size = (int *)xnmalloc(nsyms, sizeof *kernel_size);
}

/* Print the current kernel (in KERNEL_BASE). */
static void kernel_print(FILE * out)
{
	for(symbol_number i = 0; i < nsyms; ++i)
		if(kernel_size[i]) {
			fprintf(out, "kernel[%s] =\n", symbols[i]->tag);
			core_print(kernel_size[i], kernel_base[i], out);
		}
}

/* Make sure the kernel is in sane state. */
static void kernel_check(void)
{
	for(symbol_number i = 0; i < nsyms - 1; ++i)
		assert(kernel_base[i] + kernel_size[i] <= kernel_base[i + 1]);
}

static void allocate_storage(void)
{
	allocate_itemsets();
	shiftset = (state **)xnmalloc(nsyms, sizeof *shiftset);
	redset = (rule **)xnmalloc(nrules, sizeof *redset);
	state_hash_new();
	shift_symbol = bitset_create(nsyms, BITSET_FIXED);
}

static void free_storage(void)
{
	bitset_free(shift_symbol);
	SAlloc::F(redset);
	SAlloc::F(shiftset);
	SAlloc::F(kernel_base);
	SAlloc::F(kernel_size);
	SAlloc::F(kernel_items);
	state_hash_free();
}

/*------------------------------------------------------------------.
| Find which term/nterm symbols can be "shifted" in S, and for each |
| one record which items would be active after that transition.     |
| Uses the contents of itemset.                                     |
|                                                                   |
| shift_symbol is a bitset of the term/nterm symbols that can be    |
| shifted.  For each symbol in the grammar, kernel_base[symbol]     |
| points to a vector of item numbers activated if that symbol is    |
| shifted, and kernel_size[symbol] is their numbers.                |
|                                                                   |
| itemset is sorted on item index in ritem, which is sorted on rule |
| number.  Compute each kernel_base[symbol] with the same sort.     |
   `------------------------------------------------------------------*/

static void new_itemsets(state * s)
{
	if(trace_flag & trace_automaton)
		fprintf(stderr, "new_itemsets: begin: state = %d\n", s->number);
	memzero(kernel_size, nsyms * sizeof *kernel_size);
	bitset_zero(shift_symbol);
	if(trace_flag & trace_automaton) {
		fprintf(stderr, "initial kernel:\n");
		kernel_print(stderr);
	}
	for(size_t i = 0; i < nitemset; ++i)
		if(item_number_is_symbol_number(ritem[itemset[i]])) {
			if(trace_flag & trace_automaton) {
				fputs("working on: ", stderr);
				item_print(ritem + itemset[i], NULL, stderr);
				fputc('\n', stderr);
			}
			symbol_number sym = item_number_as_symbol_number(ritem[itemset[i]]);
			bitset_set(shift_symbol, sym);
			kernel_base[sym][kernel_size[sym]] = itemset[i] + 1;
			kernel_size[sym]++;
		}

	if(trace_flag & trace_automaton) {
		fprintf(stderr, "final kernel:\n");
		kernel_print(stderr);
		fprintf(stderr, "new_itemsets: end: state = %d\n\n", s->number);
	}
	kernel_check();
}

/*--------------------------------------------------------------.
| Find the state we would get to (from the current state) by    |
| shifting SYM.  Create a new state if no equivalent one exists |
| already.  Used by append_states.                              |
   `--------------------------------------------------------------*/

static state * get_state(symbol_number sym, size_t core_size, item_index * core)
{
	if(trace_flag & trace_automaton) {
		fprintf(stderr, "Entering get_state, symbol = %d (%s), core:\n", sym, symbols[sym]->tag);
		core_print(core_size, core, stderr);
		fputc('\n', stderr);
	}
	state * s = state_hash_lookup(core_size, core);
	SETIFZ(s, state_list_append(sym, core_size, core));
	if(trace_flag & trace_automaton)
		fprintf(stderr, "Exiting get_state => %d\n", s->number);
	return s;
}

/*---------------------------------------------------------------.
| Use the information computed by new_itemsets to find the state |
| numbers reached by each shift transition from S.               |
|                                                                |
| SHIFTSET is set up as a vector of those states.                |
   `---------------------------------------------------------------*/

static void append_states(state * s)
{
	if(trace_flag & trace_automaton)
		fprintf(stderr, "append_states: begin: state = %d\n", s->number);
	bitset_iterator iter;
	symbol_number sym;
	int i = 0;
	BITSET_FOR_EACH(iter, shift_symbol, sym, 0)
	{
		shiftset[i] = get_state(sym, kernel_size[sym], kernel_base[sym]);
		++i;
	}
	if(trace_flag & trace_automaton)
		fprintf(stderr, "append_states: end: state = %d\n", s->number);
}

/*----------------------------------------------------------------.
| Find which rules can be used for reduction transitions from the |
| current state and make a reductions structure for the state to  |
| record their rule numbers.                                      |
   `----------------------------------------------------------------*/

static void save_reductions(state * s)
{
	int count = 0;
	/* Find and count the active items that represent ends of rules. */
	for(size_t i = 0; i < nitemset; ++i) {
		item_number item = ritem[itemset[i]];
		if(item_number_is_rule_number(item)) {
			rule_number r = item_number_as_rule_number(item);
			redset[count++] = &rules[r];
			if(r == 0) {
				/* This is "reduce 0", i.e., accept. */
				aver(!final_state);
				final_state = s;
			}
		}
	}
	if(trace_flag & trace_automaton) {
		fprintf(stderr, "reduction[%d] = {\n", s->number);
		for(int i = 0; i < count; ++i) {
			rule_print(redset[i], NULL, stderr);
			fputc('\n', stderr);
		}
		fputs("}\n", stderr);
	}
	/* Make a reductions structure and copy the data into it.  */
	state_reductions_set(s, count, redset);
}

/*---------------.
| Build STATES.  |
   `---------------*/

static void set_states(void)
{
	states = (state **)xcalloc(nstates, sizeof *states);
	while(first_state) {
		state_list * p_this = first_state;
		/* Pessimization, but simplification of the code: make sure all
		   the states have valid transitions and reductions members,
		   even if reduced to 0.  It is too soon for errs, which are
		   computed later, but set_conflicts.  */
		state * s = p_this->state;
		if(!s->transitions)
			state_transitions_set(s, 0, 0);
		if(!s->reductions)
			state_reductions_set(s, 0, 0);
		states[s->number] = s;
		first_state = p_this->next;
		SAlloc::F(p_this);
	}
	first_state = NULL;
	last_state = NULL;
}

/*-------------------------------------------------------------------.
| Compute the LR(0) parser states (see state.h for details) from the |
| grammar.                                                           |
   `-------------------------------------------------------------------*/

void generate_states()
{
	allocate_storage();
	closure_new(nritems);
	/* Create the initial state.  The 0 at the lhs is the index of the
	   item of this initial rule.  */
	item_index initial_core = 0;
	state_list_append(0, 1, &initial_core);
	/* States are queued when they are created; process them all.  */
	for(state_list * list = first_state; list; list = list->next) {
		state * s = list->state;
		if(trace_flag & trace_automaton)
			fprintf(stderr, "Processing state %d (reached by %s)\n", s->number, symbols[s->accessing_symbol]->tag);
		/* Set up itemset for the transitions out of this state.  itemset gets a
		   vector of all the items that could be accepted next.  */
		closure(s->items, s->nitems);
		/* Record the reductions allowed out of this state.  */
		save_reductions(s);
		/* Find the itemsets of the states that shifts/gotos can reach.  */
		new_itemsets(s);
		/* Find or create the core structures for those states.  */
		append_states(s);
		/* Create the shifts structures for the shifts to those states,
		   now that the state numbers transitioning to are known.  */
		state_transitions_set(s, bitset_count(shift_symbol), shiftset);
	}
	free_storage(); /* discard various storage */
	set_states(); /* Set up STATES. */
}
