/* Output the generated parsing program for Bison.

   Copyright (C) 1984, 1986, 1989, 1992, 2000-2006, 2009-2015, 2018-2020
   Free Software Foundation, Inc.

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

#include "bison.h"
#pragma hdrstop
#include "tables.h"

/* Several tables are indexed both by state and nonterminal numbers.
   We call such an index a 'vector'; i.e., a vector is either a state
   or a nonterminal number.

   Of course vector_number_t ought to be wide enough to contain
   state_number and symbol_number.  */
typedef int vector_number;
int nvectors; // @global

#if 0 /* Not currently used.  */
static inline vector_number state_number_to_vector_number(state_number s) { return s; }
#endif

// Be ready to map a state_number to an int. 
static inline int state_number_as_int(state_number s) { return (int)(s); }

static inline vector_number symbol_number_to_vector_number(symbol_number sym)
{
	return state_number_as_int(nstates) + sym - ntokens;
}

/* FROMS and TOS are indexed by vector_number.

   If VECTOR is a nonterminal, (FROMS[VECTOR], TOS[VECTOR]) form an
   array of state numbers of the non defaulted GOTO on VECTOR.

   If VECTOR is a state, TOS[VECTOR] is the array of actions to do on
   the (array of) symbols FROMS[VECTOR].

   In both cases, TALLY[VECTOR] is the size of the arrays
   FROMS[VECTOR], TOS[VECTOR]; and WIDTH[VECTOR] =
   (FROMS[VECTOR][SIZE] - FROMS[VECTOR][0] + 1) where SIZE =
   TALLY[VECTOR].

   FROMS therefore contains symbol_number and action_number,
   TOS state_number and action_number,
   TALLY sizes,
   WIDTH differences of FROMS.

   Let base_number be the type of FROMS, TOS, and WIDTH.  */
#define BASE_MAXIMUM INT_MAX
#define BASE_MINIMUM INT_MIN

static base_number ** froms;
static base_number ** tos;
static int ** conflict_tos;
static size_t * tally;
static base_number * width;

/* For a given state, N = ACTROW[SYMBOL]:

   If N = 0, stands for 'run the default action'.
   If N = MIN, stands for 'raise a syntax error'.
   If N > 0, stands for 'shift SYMBOL and go to n'.
   If N < 0, stands for 'reduce -N'.  */
typedef int action_number;
#define ACTION_NUMBER_MINIMUM INT_MIN

static action_number * actrow; // @global

/* FROMS and TOS are reordered to be compressed.  ORDER[VECTOR] is the
   new vector number of VECTOR.  We skip 'empty' vectors (i.e.,
   TALLY[VECTOR] = 0), and call these 'entries'.  */
static vector_number * order; // @global
static int nentries; // @global
base_number * base = NULL; // @global
// 
// A distinguished value of BASE, negative infinite.  During the
// computation equals to BASE_MINIMUM, later mapped to BASE_NINF to
// keep parser tables small
// 
base_number base_ninf = 0; // @global
static bitset pos_set = NULL; // Bitset representing an integer set in the range -nstates..table_size (as an upper bound) 
static int * conflrow; // @global
int  * conflict_table; // @global
int  * conflict_list; // @global
int    conflict_list_cnt; // @global
static int conflict_list_free; // @global

/* TABLE_SIZE is the allocated size of both TABLE and CHECK.  We start
   with more or less the original hard-coded value (which was
   SHRT_MAX).  */
static int table_size = 32768; // @global
base_number * table; // @global
base_number * check; // @global
/* The value used in TABLE to denote explicit syntax errors
   (%nonassoc), a negative infinite.  First defaults to ACTION_NUMBER_MINIMUM,
   but in order to keep small tables, renumbered as TABLE_ERROR, which
   is the smallest (non error) value minus 1.  */
base_number table_ninf = 0; // @global
static int lowzero; // @global
int high; // @global
state_number * yydefgoto; // @global
rule_number * yydefact; // @global

/*-------------------------------------------------------------------.
| If TABLE, CONFLICT_TABLE, and CHECK are too small to be addressed  |
| at DESIRED, grow them.  TABLE[DESIRED] can be used, so the desired |
| size is at least DESIRED + 1.                                      |
   `-------------------------------------------------------------------*/

static void table_grow(int desired)
{
	int old_size = table_size;
	while(table_size <= desired)
		table_size *= 2;
	if(trace_flag & trace_resource)
		fprintf(stderr, "growing tables from %d to %d\n", old_size, table_size);
	table = (base_number *)xnrealloc(table, table_size, sizeof(*table));
	memzero(table + old_size, sizeof *table * (table_size - old_size));
	conflict_table = (int *)xnrealloc(conflict_table, table_size, sizeof(*conflict_table));
	memzero(conflict_table + old_size, sizeof *conflict_table * (table_size - old_size));
	check = (base_number *)xnrealloc(check, table_size, sizeof(*check));
	for(int i = old_size; i < table_size; ++i)
		check[i] = -1;
	bitset_resize(pos_set, table_size + nstates);
}

/*-------------------------------------------------------------------.
| For GLR parsers, for each conflicted token in S, as indicated      |
| by non-zero entries in CONFLROW, create a list of possible         |
| reductions that are alternatives to the shift or reduction         |
| currently recorded for that token in S.  Store the alternative     |
| reductions followed by a 0 in CONFLICT_LIST, updating              |
| CONFLICT_LIST_CNT, and storing an index to the start of the list   |
| back into CONFLROW.                                                |
   `-------------------------------------------------------------------*/

static void conflict_row(state * s)
{
	if(nondeterministic_parser) {
		const reductions * reds = s->reductions;
		for(state_number j = 0; j < ntokens; j += 1) {
			if(conflrow[j]) {
				conflrow[j] = conflict_list_cnt;
				// Find all reductions for token J, and record all that do not match ACTROW[J]. 
				for(int i = 0; i < reds->num; i += 1)
					if(bitset_test(reds->lookaheads[i], j) && (actrow[j] != rule_number_as_item_number(reds->rules[i]->number))) {
						aver(0 < conflict_list_free);
						conflict_list[conflict_list_cnt] = reds->rules[i]->number + 1;
						conflict_list_cnt += 1;
						conflict_list_free -= 1;
					}
				/* Leave a 0 at the end.  */
				aver(0 < conflict_list_free);
				conflict_list[conflict_list_cnt] = 0;
				conflict_list_cnt += 1;
				conflict_list_free -= 1;
			}
		}
	}
}

/*------------------------------------------------------------------.
| Decide what to do for each type of token if seen as the           |
| lookahead in specified state.  The value returned is used as the  |
| default action (yydefact) for the state.  In addition, ACTROW is  |
| filled with what to do for each kind of token, index by symbol    |
| number, with zero meaning do the default action.  The value       |
| ACTION_NUMBER_MINIMUM, a very negative number, means this         |
| situation is an error.  The parser recognizes this value          |
| specially.                                                        |
|                                                                   |
| This is where conflicts are resolved.  The loop over lookahead    |
| rules considered lower-numbered rules last, and the last rule     |
| considered that likes a token gets to handle it.                  |
|                                                                   |
| For GLR parsers, also sets CONFLROW[SYM] to an index into         |
| CONFLICT_LIST iff there is an unresolved conflict (s/r or r/r)    |
| with symbol SYM. The default reduction is not used for a symbol   |
| that has any such conflicts.                                      |
   `------------------------------------------------------------------*/

static rule * action_row(state * s)
{
	for(state_number i = 0; i < ntokens; i++)
		actrow[i] = conflrow[i] = 0;
	reductions * reds = s->reductions;
	bool conflicted = false;
	if(reds->lookaheads)
		/* loop over all the rules available here which require
		   lookahead (in reverse order to give precedence to the first
		   rule) */
		for(int i = reds->num - 1; 0 <= i; --i) {
			// and find each token which the rule finds acceptable to come next 
			bitset_iterator biter;
			int j;
			BITSET_FOR_EACH(biter, reds->lookaheads[i], j, 0)
			{
				// and record this rule as the rule to use if that token follows.
				if(actrow[j] != 0) {
					conflicted = true;
					conflrow[j] = 1;
				}
				actrow[j] = rule_number_as_item_number(reds->rules[i]->number);
			}
		}
	/* Now see which tokens are allowed for shifts in this state.  For
	   them, record the shift as the thing to do.  So shift is preferred
	   to reduce.  */
	transitions * trans = s->transitions;
	/* Set to nonzero to inhibit having any default reduction.  */
	bool nodefault = false;
	{
		int i;
		FOR_EACH_SHIFT(trans, i)
		{
			symbol_number sym = TRANSITION_SYMBOL(trans, i);
			state * shift_state = trans->states[i];
			if(actrow[sym] != 0) {
				conflicted = true;
				conflrow[sym] = 1;
			}
			actrow[sym] = state_number_as_int(shift_state->number);
			// Do not use any default reduction if there is a shift for error 
			if(sym == errtoken->content->number)
				nodefault = true;
		}
	}
	/* See which tokens are an explicit error in this state (due to
	   %nonassoc).  For them, record ACTION_NUMBER_MINIMUM as the
	   action.  */
	errs * errp = s->errs;
	for(int i = 0; i < errp->num; i++) {
		Symbol * sym = errp->symbols[i];
		actrow[sym->content->number] = ACTION_NUMBER_MINIMUM;
	}
	/* Turn off default reductions where requested by the user.  See
	   state_lookaheads_count in lalr.c to understand when states are
	   labeled as consistent.  */
	{
		char * default_reductions = muscle_percent_define_get("lr.default-reduction");
		if(STRNEQ(default_reductions, "most") && !s->consistent)
			nodefault = true;
		SAlloc::F(default_reductions);
	}
	// Now find the most common reduction and make it the default action for this state. 
	rule * default_reduction = NULL;
	if(reds->num >= 1 && !nodefault) {
		if(s->consistent)
			default_reduction = reds->rules[0];
		else {
			int max = 0;
			for(int i = 0; i < reds->num; i++) {
				int count = 0;
				rule * r = reds->rules[i];
				for(symbol_number j = 0; j < ntokens; j++)
					if(actrow[j] == rule_number_as_item_number(r->number))
						count++;
				if(count > max) {
					max = count;
					default_reduction = r;
				}
			}
			/* GLR parsers need space for conflict lists, so we can't
			   default conflicted entries.  For non-conflicted entries
			   or as long as we are not building a GLR parser,
			   actions that match the default are replaced with zero,
			   which means "use the default". */

			if(0 < max)
				for(symbol_number j = 0; j < ntokens; j++)
					if(actrow[j] == rule_number_as_item_number(default_reduction->number) && !(nondeterministic_parser && conflrow[j]))
						actrow[j] = 0;
		}
	}
	/* If have no default reduction, the default is an error.
	   So replace any action which says "error" with "use default".  */
	if(!default_reduction)
		for(symbol_number i = 0; i < ntokens; i++)
			if(actrow[i] == ACTION_NUMBER_MINIMUM)
				actrow[i] = 0;
	if(conflicted)
		conflict_row(s);
	return default_reduction;
}

/*----------------------------------------.
| Set FROMS, TOS, TALLY and WIDTH for S.  |
   `----------------------------------------*/

static void save_row(state_number s)
{
	/* Number of non default actions in S.  */
	size_t count = 0;
	for(symbol_number i = 0; i < ntokens; i++)
		if(actrow[i] != 0)
			count++;
	if(count) {
		/* Allocate non defaulted actions.  */
		base_number * sp1 = froms[s] = (base_number *)xnmalloc(count, sizeof *sp1);
		base_number * sp2 = tos[s] = (base_number *)xnmalloc(count, sizeof *sp2);
		int * sp3 = conflict_tos[s] = nondeterministic_parser ? (int *)xnmalloc(count, sizeof *sp3) : NULL;
		/* Store non defaulted actions.  */
		for(symbol_number i = 0; i < ntokens; i++)
			if(actrow[i] != 0) {
				*sp1++ = i;
				*sp2++ = actrow[i];
				if(nondeterministic_parser)
					*sp3++ = conflrow[i];
			}

		tally[s] = count;
		width[s] = sp1[-1] - froms[s][0] + 1;
	}
}

/*------------------------------------------------------------------.
| Figure out the actions for the specified state, indexed by        |
| lookahead token kind.                                             |
|                                                                   |
| The YYDEFACT table is output now.  The detailed info is saved for |
| putting into YYTABLE later.                                       |
   `------------------------------------------------------------------*/

static void token_actions()
{
	int nconflict = nondeterministic_parser ? conflicts_total_count() : 0;
	yydefact = (rule_number *)xnmalloc(nstates, sizeof *yydefact);
	actrow = (action_number *)xnmalloc(ntokens, sizeof *actrow);
	conflrow = (int *)xnmalloc(ntokens, sizeof *conflrow);
	conflict_list = (int *)xnmalloc(1 + 2 * nconflict, sizeof *conflict_list);
	conflict_list_free = 2 * nconflict;
	conflict_list_cnt = 1;

	/* Find the rules which are reduced.  */
	if(!nondeterministic_parser)
		for(rule_number r = 0; r < nrules; ++r)
			rules[r].useful = false;

	for(state_number i = 0; i < nstates; ++i) {
		rule * default_reduction = action_row(states[i]);
		yydefact[i] = default_reduction ? default_reduction->number + 1 : 0;
		save_row(i);

		/* Now that the parser was computed, we can find which rules are
		   really reduced, and which are not because of SR or RR
		   conflicts.  */
		if(!nondeterministic_parser) {
			for(symbol_number j = 0; j < ntokens; ++j)
				if(actrow[j] < 0 && actrow[j] != ACTION_NUMBER_MINIMUM)
					rules[item_number_as_rule_number(actrow[j])].useful = true;
			if(yydefact[i])
				rules[yydefact[i] - 1].useful = true;
		}
	}
	SAlloc::F(actrow);
	SAlloc::F(conflrow);
}

/*------------------------------------------------------------------.
| Compute FROMS[VECTOR], TOS[VECTOR], TALLY[VECTOR], WIDTH[VECTOR], |
| i.e., the information related to non defaulted GOTO on the nterm  |
| SYM.                                                              |
|                                                                   |
| DEFAULT_STATE is the principal destination on SYM, i.e., the      |
| default GOTO destination on SYM.                                  |
   `------------------------------------------------------------------*/

static void save_column(symbol_number sym, state_number default_state)
{
	const goto_number begin = goto_map[sym - ntokens];
	const goto_number end = goto_map[sym - ntokens + 1];

	/* Number of non default GOTO.  */
	size_t count = 0;
	for(goto_number i = begin; i < end; i++)
		if(to_state[i] != default_state)
			count++;

	if(count) {
		/* Allocate room for non defaulted gotos.  */
		vector_number symno = symbol_number_to_vector_number(sym);
		base_number * sp1 = froms[symno] = (base_number *)xnmalloc(count, sizeof *sp1);
		base_number * sp2 = tos[symno] = (base_number *)xnmalloc(count, sizeof *sp2);
		/* Store the state numbers of the non defaulted gotos.  */
		for(goto_number i = begin; i < end; i++)
			if(to_state[i] != default_state) {
				*sp1++ = from_state[i];
				*sp2++ = to_state[i];
			}

		tally[symno] = count;
		width[symno] = sp1[-1] - froms[symno][0] + 1;
	}
}

/*----------------------------------------------------------------.
| The default state for SYM: the state which is 'the' most common |
| GOTO destination on SYM (an nterm).                             |
   `----------------------------------------------------------------*/

static state_number default_goto(symbol_number sym, size_t state_count[])
{
	const goto_number begin = goto_map[sym - ntokens];
	const goto_number end = goto_map[sym - ntokens + 1];
	state_number res = -1;

	if(begin != end) {
		for(state_number s = 0; s < nstates; s++)
			state_count[s] = 0;

		for(goto_number i = begin; i < end; i++)
			state_count[to_state[i]]++;

		size_t max = 0;
		for(state_number s = 0; s < nstates; s++)
			if(max < state_count[s]) {
				max = state_count[s];
				res = s;
			}
	}
	return res;
}

/*-------------------------------------------------------------------.
| Figure out what to do after reducing with each rule, depending on  |
| the saved state from before the beginning of parsing the data that |
| matched this rule.                                                 |
|                                                                    |
| The YYDEFGOTO table is output now.  The detailed info is saved for |
| putting into YYTABLE later.                                        |
   `-------------------------------------------------------------------*/

static void goto_actions()
{
	size_t * state_count = (size_t *)xnmalloc(nstates, sizeof *state_count);
	yydefgoto = (state_number *)xnmalloc(nnterms, sizeof *yydefgoto);
	/* For a given nterm I, STATE_COUNT[S] is the number of times there
	   is a GOTO to S on I.  */
	for(symbol_number i = ntokens; i < nsyms; ++i) {
		state_number default_state = default_goto(i, state_count);
		save_column(i, default_state);
		yydefgoto[i - ntokens] = default_state;
	}
	SAlloc::F(state_count);
}

/*------------------------------------------------------------------.
| Compute ORDER, a reordering of vectors, in order to decide how to |
| pack the actions and gotos information into yytable.              |
   `------------------------------------------------------------------*/

static void sort_actions()
{
	nentries = 0;
	for(int i = 0; i < nvectors; i++) {
		if(0 < tally[i]) {
			const size_t t = tally[i];
			const int w = width[i];
			int j = nentries - 1;
			while(0 <= j && width[order[j]] < w)
				j--;
			while(0 <= j && width[order[j]] == w && tally[order[j]] < t)
				j--;
			for(int k = nentries - 1; k > j; k--)
				order[k + 1] = order[k];
			order[j + 1] = i;
			nentries++;
		}
	}
}

/* If VECTOR is a state whose actions (reflected by FROMS, TOS, TALLY
   and WIDTH of VECTOR) are common to a previous state, return this
   state number.

   In any other case, return -1.  */

static state_number matching_state(vector_number vector)
{
	vector_number i = order[vector];
	// If VECTOR is a nterm, return -1. 
	if(i < nstates) {
		size_t t = tally[i];
		int w = width[i];
		// If VECTOR has GLR conflicts, return -1 
		if(conflict_tos[i] != NULL)
			for(size_t j = 0; j < t; j += 1)
				if(conflict_tos[i][j] != 0)
					return -1;
		for(int prev = vector - 1; 0 <= prev; prev--) {
			vector_number j = order[prev];
			// Given how ORDER was computed, if the WIDTH or TALLY is different, there cannot be a matching state. 
			if(width[j] != w || tally[j] != t)
				return -1;
			else {
				bool match = true;
				for(size_t k = 0; match && k < t; k++)
					if(tos[j][k] != tos[i][k] || froms[j][k] != froms[i][k] || (conflict_tos[j] != NULL && conflict_tos[j][k] != 0))
						match = false;
				if(match)
					return j;
			}
		}
	}
	return -1;
}

static base_number pack_vector(vector_number vector)
{
	vector_number i = order[vector];
	size_t t = tally[i];
	base_number * from = froms[i];
	base_number * to = tos[i];
	const int * conflict_to = conflict_tos[i];
	aver(t != 0);
	for(base_number res = lowzero - from[0];; res++) {
		bool ok = true;
		aver(res < table_size);
		{
			for(size_t k = 0; ok && k < t; k++) {
				int loc = res + state_number_as_int(from[k]);
				if(table_size <= loc)
					table_grow(loc);
				if(table[loc] != 0)
					ok = false;
			}
			if(ok && bitset_test(pos_set, nstates + res))
				ok = false;
		}
		if(ok) {
			int loc PACIFY_CC( = -1);
			for(size_t k = 0; k < t; k++) {
				loc = res + state_number_as_int(from[k]);
				table[loc] = to[k];
				if(nondeterministic_parser && conflict_to != NULL)
					conflict_table[loc] = conflict_to[k];
				check[loc] = from[k];
			}
			while(table[lowzero] != 0)
				lowzero++;
			if(high < loc)
				high = loc;
			aver(BASE_MINIMUM <= res && res <= BASE_MAXIMUM);
			return res;
		}
	}
}

/*-------------------------------------------------------------.
| Remap the negative infinite in TAB from NINF to the greatest |
| possible smallest value.  Return it.                         |
|                                                              |
| In most case this allows us to use shorts instead of ints in |
| parsers.                                                     |
   `-------------------------------------------------------------*/

static base_number table_ninf_remap(base_number tab[], int size, base_number ninf)
{
	base_number res = 0;
	for(int i = 0; i < size; i++)
		if(tab[i] < res && tab[i] != ninf)
			res = tab[i];
	--res;
	for(int i = 0; i < size; i++)
		if(tab[i] == ninf)
			tab[i] = res;
	return res;
}

static void pack_table()
{
	base = (base_number *)xnmalloc(nvectors, sizeof *base);
	pos_set = bitset_create(table_size + nstates, BITSET_FRUGAL);
	table = (base_number *)xcalloc(table_size, sizeof *table);
	conflict_table = (int *)xcalloc(table_size, sizeof *conflict_table);
	check = (base_number *)xnmalloc(table_size, sizeof *check);
	lowzero = 0;
	high = 0;
	for(int i = 0; i < nvectors; i++)
		base[i] = BASE_MINIMUM;
	for(int i = 0; i < table_size; i++)
		check[i] = -1;
	for(int i = 0; i < nentries; i++) {
		state_number s = matching_state(i);
		base_number place;
		if(s < 0)
			place = pack_vector(i); /* A new set of state actions, or a nonterminal.  */
		else
			place = base[s]; /* Action of I were already coded for S.  */
		/* Store PLACE into POS_SET.  PLACE might not belong to the set
		   of possible values for instance with useless tokens.  It
		   would be more satisfying to eliminate the need for this
		   'if'.  */
		if(0 <= nstates + place)
			bitset_set(pos_set, nstates + place);
		base[order[i]] = place;
	}
	/* Use the greatest possible negative infinites.  */
	base_ninf = table_ninf_remap(base, nvectors, BASE_MINIMUM);
	table_ninf = table_ninf_remap(table, high + 1, ACTION_NUMBER_MINIMUM);
	bitset_free(pos_set);
}

/*-----------------------------------------------------------------.
| Compute and output yydefact, yydefgoto, yypact, yypgoto, yytable |
| and yycheck.                                                     |
   `-----------------------------------------------------------------*/

void tables_generate()
{
	/* This is a poor way to make sure the sizes are properly
	   correlated.  In particular the signedness is not taken into
	   account.  But it's not useless.  */
	verify(sizeof nstates <= sizeof nvectors);
	verify(sizeof nnterms <= sizeof nvectors);
	nvectors = state_number_as_int(nstates) + nnterms;
	froms = (base_number **)xcalloc(nvectors, sizeof *froms);
	tos = (base_number **)xcalloc(nvectors, sizeof *tos);
	conflict_tos = (int **)xcalloc(nvectors, sizeof *conflict_tos);
	tally = (size_t *)xcalloc(nvectors, sizeof *tally);
	width = (base_number *)xnmalloc(nvectors, sizeof *width);

	token_actions();
	goto_actions();
	SAlloc::F(goto_map);
	SAlloc::F(from_state);
	SAlloc::F(to_state);
	order = (vector_number *)xcalloc(nvectors, sizeof *order);
	sort_actions();
	pack_table();
	SAlloc::F(order);
	SAlloc::F(tally);
	SAlloc::F(width);
	for(int i = 0; i < nvectors; i++) {
		SAlloc::F(froms[i]);
		SAlloc::F(tos[i]);
		SAlloc::F(conflict_tos[i]);
	}
	SAlloc::F(froms);
	SAlloc::F(tos);
	SAlloc::F(conflict_tos);
}
// 
// Free the parser tables
// 
void tables_free()
{
	SAlloc::F(base);
	SAlloc::F(conflict_table);
	SAlloc::F(conflict_list);
	SAlloc::F(table);
	SAlloc::F(check);
	SAlloc::F(yydefgoto);
	SAlloc::F(yydefact);
}
