/* Output the generated parsing program for Bison.

   Copyright (C) 1984, 1986, 1989, 1992, 2000-2015, 2018-2020 Free
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

#include "bison.h"
#pragma hdrstop
#include "tables.h"

static struct obstack format_obstack;

/*-------------------------------------------------------------------.
| Create a function NAME which associates to the muscle NAME the     |
| result of formatting the FIRST and then TABLE_DATA[BEGIN..END[ (of |
| TYPE), and to the muscle NAME_max, the max value of the            |
| TABLE_DATA.                                                        |
|                                                                    |
| For the typical case of outputting a complete table from 0, pass   |
| TABLE[0] as FIRST, and 1 as BEGIN.  For instance                   |
| muscle_insert_base_table ("pact", base, base[0], 1, nstates);      |
   `-------------------------------------------------------------------*/

#define GENERATE_MUSCLE_INSERT_TABLE(Name, Type)                        \
                                                                        \
	static void Name(char const * name, Type *table_data, Type first, int begin, int end) \
	{                                                                       \
		Type min = first;                                                     \
		Type max = first;                                                     \
		int j = 1;                                                            \
		obstack_printf(&format_obstack, "%6d", first);                       \
		for(int i = begin; i < end; ++i) {  \
			obstack_1grow(&format_obstack, ',');                             \
			if(j >= 10) { \
				obstack_sgrow(&format_obstack, "\n  ");                      \
				j = 1;                                                        \
			}                                                               \
			else                                                              \
				++j;                                                            \
			obstack_printf(&format_obstack, "%6d", table_data[i]);           \
			if(table_data[i] < min)                                          \
				min = table_data[i];                                            \
			if(max < table_data[i])                                          \
				max = table_data[i];                                            \
		}                                                                   \
		muscle_insert(name, obstack_finish0(&format_obstack));              \
		long lmin = min;                                                      \
		long lmax = max;                                                      \
		/* Build 'NAME_min' and 'NAME_max' in the obstack. */                 \
		obstack_printf(&format_obstack, "%s_min", name);                     \
		MUSCLE_INSERT_LONG_INT(obstack_finish0(&format_obstack), lmin);     \
		obstack_printf(&format_obstack, "%s_max", name);                     \
		MUSCLE_INSERT_LONG_INT(obstack_finish0(&format_obstack), lmax);     \
	}

GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_int_table, int)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_base_table, base_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_rule_number_table, rule_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_symbol_number_table, symbol_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_item_number_table, item_number)
GENERATE_MUSCLE_INSERT_TABLE(muscle_insert_state_number_table, state_number)

/*----------------------------------------------------------------.
| Print to OUT a representation of CP quoted and escaped for M4.  |
   `----------------------------------------------------------------*/

static void output_escaped(FILE * out, const char * cp)
{
	for(; *cp; cp++)
		switch(*cp) {
			case '$': fputs("$][", out); break;
			case '@': fputs("@@",  out); break;
			case '[': fputs("@{",  out); break;
			case ']': fputs("@}",  out); break;
			default:  fputc(*cp,   out); break;
		}
}

static void output_quoted(FILE * out, char const * cp)
{
	fprintf(out, "[[");
	output_escaped(out, cp);
	fprintf(out, "]]");
}

/*----------------------------------------------------------------.
| Print to OUT a representation of STRING quoted and escaped both |
| for C and M4.                                                   |
   `----------------------------------------------------------------*/

static void string_output(FILE * out, char const * string)
{
	output_quoted(out, quotearg_style(c_quoting_style, string));
}

/* Store in BUFFER a copy of SRC where trigraphs are escaped, return
   the size of the result (including the final NUL).  If called with
   BUFFERSIZE = 0, returns the needed size for BUFFER.  */
static ptrdiff_t escape_trigraphs(char * buffer, ptrdiff_t buffersize, const char * src)
{
#define STORE(c)                                \
	do {                                          \
		if(res < buffersize)                     \
			buffer[res] = (c);                      \
		++res;                                    \
	} while(0)
	ptrdiff_t res = 0;
	for(ptrdiff_t i = 0, len = strlen(src); i < len; ++i) {
		if(i + 2 < len && src[i] == '?' && src[i+1] == '?') {
			switch(src[i+2]) {
				case '!': case '\'':
				case '(': case ')': case '-': case '/':
				case '<': case '=': case '>':
				    i += 1;
				    STORE('?');
				    STORE('"');
				    STORE('"');
				    STORE('?');
				    continue;
			}
		}
		STORE(src[i]);
	}
	STORE('\0');
#undef STORE
	return res;
}

/* Same as xstrdup, except that trigraphs are escaped.  */
static char * xescape_trigraphs(const char * src)
{
	ptrdiff_t bufsize = escape_trigraphs(NULL, 0, src);
	char * buf = xcharalloc(bufsize);
	escape_trigraphs(buf, bufsize, src);
	return buf;
}

/* The tag to show in the generated parsers.  Use "end of file" rather
   than "$end".  But keep "$end" in the reports, it's shorter and more
   consistent.  Support i18n if the user already uses it.  */
static const char * symbol_tag(const Symbol * sym)
{
	const bool eof_is_user_defined = !eoftoken->alias || STRNEQ(eoftoken->alias->tag, "$end");
	if(!eof_is_user_defined && sym->content == eoftoken->content)
		return "\"end of file\"";
	else if(sym->content == undeftoken->content)
		return "\"invalid token\"";
	else
		return sym->tag;
}

/* Generate the b4_<MUSCLE_NAME> (e.g., b4_tname) table with the
   symbol names (aka tags). */

static void prepare_symbol_names(char const * muscle_name)
{
	// Whether to add a pair of quotes around the name.
	const bool quote = sstreq(muscle_name, "tname");
	bool has_translations = false;
	/* We assume that the table will be output starting at column 2. */
	int col = 2;
	struct quoting_options * qo = clone_quoting_options(0);
	set_quoting_style(qo, c_quoting_style);
	set_quoting_flags(qo, QA_SPLIT_TRIGRAPHS);
	for(int i = 0; i < nsyms; i++) {
		const char * tag = symbol_tag(symbols[i]);
		bool translatable = !quote && symbols[i]->translatable;
		if(translatable)
			has_translations = true;

		char * cp
			= tag[0] == '"' && !quote
		    ? xescape_trigraphs(tag)
		    : quotearg_alloc(tag, -1, qo);
		/* Width of the next token, including the two quotes, the
		   comma and the space.  */
		int width
			= mbswidth(cp, 0) + 2
		    + (translatable ? strlen("N_()") : 0);

		if(col + width > 75) {
			obstack_sgrow(&format_obstack, "\n ");
			col = 1;
		}

		if(i)
			obstack_1grow(&format_obstack, ' ');
		if(translatable)
			obstack_sgrow(&format_obstack, "]b4_symbol_translate([");
		obstack_escape(&format_obstack, cp);
		if(translatable)
			obstack_sgrow(&format_obstack, "])[");
		SAlloc::F(cp);
		obstack_1grow(&format_obstack, ',');
		col += width;
	}
	SAlloc::F(qo);
	obstack_sgrow(&format_obstack, " ]b4_null[");

	/* Finish table and store. */
	muscle_insert(muscle_name, obstack_finish0(&format_obstack));

	/* Announce whether translation support is needed.  */
	MUSCLE_INSERT_BOOL("has_translations_flag", has_translations);
}

/*------------------------------------------------------------------.
| Prepare the muscles related to the symbols: translate, tname, and |
| toknum.                                                           |
   `------------------------------------------------------------------*/

static void prepare_symbols()
{
	MUSCLE_INSERT_INT("tokens_number", ntokens);
	MUSCLE_INSERT_INT("nterms_number", nnterms);
	MUSCLE_INSERT_INT("symbols_number", nsyms);
	MUSCLE_INSERT_INT("code_max", max_code);

	muscle_insert_symbol_number_table("translate",
	    token_translations,
	    token_translations[0],
	    1, max_code + 1);

	/* tname -- token names.  */
	prepare_symbol_names("tname");
	prepare_symbol_names("symbol_names");

	/* translatable -- whether a token is translatable. */
	{
		bool translatable = false;
		for(int i = 0; i < ntokens; ++i)
			if(symbols[i]->translatable) {
				translatable = true;
				break;
			}
		if(translatable) {
			int * values = (int *)xnmalloc(nsyms, sizeof *values);
			for(int i = 0; i < ntokens; ++i)
				values[i] = symbols[i]->translatable;
			muscle_insert_int_table("translatable", values,
			    values[0], 1, ntokens);
			SAlloc::F(values);
		}
	}

	/* Output YYTOKNUM. */
	{
		int * values = (int *)xnmalloc(ntokens, sizeof *values);
		for(int i = 0; i < ntokens; ++i)
			values[i] = symbols[i]->content->code;
		muscle_insert_int_table("toknum", values, values[0], 1, ntokens);
		SAlloc::F(values);
	}
}

/*-------------------------------------------------------------.
| Prepare the muscles related to the rules: rhs, prhs, r1, r2, |
| rline, dprec, merger, immediate.                             |
   `-------------------------------------------------------------*/

static void prepare_rules()
{
	int * prhs = (int *)xnmalloc(nrules, sizeof *prhs);
	item_number * rhs = (item_number *)xnmalloc(nritems, sizeof *rhs);
	int * rline = (int *)xnmalloc(nrules, sizeof *rline);
	symbol_number * r1 = (symbol_number *)xnmalloc(nrules, sizeof *r1);
	int * r2 = (int *)xnmalloc(nrules, sizeof *r2);
	int * dprec = (int *)xnmalloc(nrules, sizeof *dprec);
	int * merger = (int *)xnmalloc(nrules, sizeof *merger);
	int * immediate = (int *)xnmalloc(nrules, sizeof *immediate);
	/* Index in RHS.  */
	int i = 0;
	for(rule_number r = 0; r < nrules; ++r) {
		/* Index of rule R in RHS. */
		prhs[r] = i;
		/* RHS of the rule R. */
		for(item_number * rhsp = rules[r].rhs; 0 <= *rhsp; ++rhsp)
			rhs[i++] = *rhsp;
		/* Separator in RHS. */
		rhs[i++] = -1;

		/* Line where rule was defined. */
		rline[r] = rules[r].location.start.line;
		/* LHS of the rule R. */
		r1[r] = rules[r].lhs->number;
		/* Length of rule R's RHS. */
		r2[r] = rule_rhs_length(&rules[r]);
		/* Dynamic precedence (GLR).  */
		dprec[r] = rules[r].dprec;
		/* Merger-function index (GLR).  */
		merger[r] = rules[r].merger;
		/* Immediate reduction flags (GLR).  */
		immediate[r] = rules[r].is_predicate;
	}
	aver(i == nritems);

	muscle_insert_item_number_table("rhs", rhs, ritem[0], 1, nritems);
	muscle_insert_int_table("prhs", prhs, 0, 0, nrules);
	muscle_insert_int_table("rline", rline, 0, 0, nrules);
	muscle_insert_symbol_number_table("r1", r1, 0, 0, nrules);
	muscle_insert_int_table("r2", r2, 0, 0, nrules);
	muscle_insert_int_table("dprec", dprec, 0, 0, nrules);
	muscle_insert_int_table("merger", merger, 0, 0, nrules);
	muscle_insert_int_table("immediate", immediate, 0, 0, nrules);

	MUSCLE_INSERT_INT("rules_number", nrules);
	MUSCLE_INSERT_INT("max_left_semantic_context", max_left_semantic_context);

	SAlloc::F(prhs);
	SAlloc::F(rhs);
	SAlloc::F(rline);
	SAlloc::F(r1);
	SAlloc::F(r2);
	SAlloc::F(dprec);
	SAlloc::F(merger);
	SAlloc::F(immediate);
}

/*--------------------------------------------.
| Prepare the muscles related to the states.  |
   `--------------------------------------------*/

static void prepare_states()
{
	symbol_number * values = (symbol_number *)xnmalloc(nstates, sizeof *values);
	for(state_number i = 0; i < nstates; ++i)
		values[i] = states[i]->accessing_symbol;
	muscle_insert_symbol_number_table("stos", values, 0, 1, nstates);
	SAlloc::F(values);
	MUSCLE_INSERT_INT("last", high);
	MUSCLE_INSERT_INT("final_state_number", final_state->number);
	MUSCLE_INSERT_INT("states_number", nstates);
}

/*-------------------------------------------------------.
| Compare two symbols by type-name, and then by number.  |
   `-------------------------------------------------------*/

static int symbol_type_name_cmp(const Symbol ** lhs, const Symbol ** rhs)
{
	int res = uniqstr_cmp((*lhs)->content->type_name, (*rhs)->content->type_name);
	if(!res)
		res = (*lhs)->content->number - (*rhs)->content->number;
	return res;
}

/*----------------------------------------------------------------.
| Return a (malloc'ed) table of the symbols sorted by type-name.  |
   `----------------------------------------------------------------*/

static Symbol ** symbols_by_type_name()
{
	typedef int (* qcmp_type)(const void *, const void *);
	Symbol ** res = (Symbol **)xmemdup(symbols, nsyms * sizeof *res);
	qsort(res, nsyms, sizeof *res, (qcmp_type) &symbol_type_name_cmp);
	return res;
}

/*------------------------------------------------------------------.
| Define b4_type_names, which is a list of (lists of the numbers of |
| symbols with same type-name).                                     |
   `------------------------------------------------------------------*/

static void type_names_output(FILE * out)
{
	Symbol ** syms = symbols_by_type_name();
	fputs("m4_define([b4_type_names],\n[", out);
	for(int i = 0; i < nsyms; /* nothing */) {
		/* The index of the first symbol of the current type-name.  */
		int i0 = i;
		fputs(i ? ",\n[" : "[", out);
		for(; i < nsyms
		    && syms[i]->content->type_name == syms[i0]->content->type_name; ++i)
			fprintf(out, "%s%d", i != i0 ? ", " : "", syms[i]->content->number);
		fputs("]", out);
	}
	fputs("])\n\n", out);
	SAlloc::F(syms);
}

/*-------------------------------------.
| The list of all the symbol numbers.  |
   `-------------------------------------*/

static void symbol_numbers_output(FILE * out)
{
	fputs("m4_define([b4_symbol_numbers],\n[", out);
	for(int i = 0; i < nsyms; ++i)
		fprintf(out, "%s[%d]", i ? ", " : "", i);
	fputs("])\n\n", out);
}

/*-------------------------------------------.
| Output the user reduction actions to OUT.  |
   `-------------------------------------------*/

static void rule_output(const rule * r, FILE * out)
{
	output_escaped(out, r->lhs->symbol->tag);
	fputc(':', out);
	if(0 <= *r->rhs)
		for(item_number * rhsp = r->rhs; 0 <= *rhsp; ++rhsp) {
			fputc(' ', out);
			output_escaped(out, symbols[*rhsp]->tag);
		}
	else
		fputs(" %empty", out);
}

static void user_actions_output(FILE * out)
{
	fputs("m4_define([b4_actions], \n[", out);
	for(rule_number r = 0; r < nrules; ++r)
		if(rules[r].action) {
			/* The useless "" is there to pacify syntax-check.  */
			fprintf(out, "%s" "(%d, [",
			    rules[r].is_predicate ? "b4_predicate_case" : "b4_case",
			    r + 1);
			if(!no_lines_flag) {
				fprintf(out, "b4_syncline(%d, ",
				    rules[r].action_loc.start.line);
				string_output(out, rules[r].action_loc.start.file);
				fprintf(out, ")dnl\n");
			}
			fprintf(out, "[%*s%s]],\n[[",
			    rules[r].action_loc.start.column - 1, "",
			    rules[r].action);
			rule_output(&rules[r], out);
			fprintf(out, "]])\n\n");
		}
	fputs("])\n\n", out);
}

/*------------------------------------.
| Output the merge functions to OUT.  |
   `------------------------------------*/

static void merger_output(FILE * out)
{
	fputs("m4_define([b4_mergers], \n[[", out);
	int n;
	merger_list * p;
	for(n = 1, p = merge_functions; p != NULL; n += 1, p = p->next) {
		if(p->type[0] == '\0')
			fprintf(out, "  case %d: *yy0 = %s (*yy0, *yy1); break;\n",
			    n, p->name);
		else
			fprintf(out, "  case %d: yy0->%s = %s (*yy0, *yy1); break;\n",
			    n, p->type, p->name);
	}
	fputs("]])\n\n", out);
}

/*---------------------------------------------.
| Prepare the muscles for symbol definitions.  |
   `---------------------------------------------*/

static void prepare_symbol_definitions()
{
	/* Map "orig NUM" to new numbers.  See data/README.  */
	for(symbol_number i = ntokens; i < nsyms + nuseless_nonterminals; ++i) {
		obstack_printf(&format_obstack, "symbol(orig %d, number)", i);
		const char * key = obstack_finish0(&format_obstack);
		MUSCLE_INSERT_INT(key, nterm_map ? nterm_map[i - ntokens] : i);
	}

	for(int i = 0; i < nsyms; ++i) {
		Symbol * sym = symbols[i];
		const char * key;

#define SET_KEY(Entry)                                          \
	obstack_printf(&format_obstack, "symbol(%d, %s)", i, Entry); \
	key = obstack_finish0(&format_obstack);

#define SET_KEY2(Entry, Suffix)                                 \
	obstack_printf(&format_obstack, "symbol(%d, %s_%s)", i, Entry, Suffix); \
	key = obstack_finish0(&format_obstack);

		/* Whether the symbol has an identifier.  */
		const char * id = symbol_id_get(sym);
		SET_KEY("has_id");
		MUSCLE_INSERT_INT(key, !!id);

		/* Its identifier.  */
		SET_KEY("id");
		MUSCLE_INSERT_STRING(key, id ? id : "");

		/* Its tag.  Typically for documentation purpose.  */
		SET_KEY("tag");
		MUSCLE_INSERT_STRING(key, symbol_tag(sym));

		SET_KEY("code");
		MUSCLE_INSERT_INT(key, sym->content->code);

		SET_KEY("is_token");
		MUSCLE_INSERT_INT(key, i < ntokens);

		SET_KEY("number");
		MUSCLE_INSERT_INT(key, sym->content->number);

		SET_KEY("has_type");
		MUSCLE_INSERT_INT(key, !!sym->content->type_name);

		SET_KEY("type");
		MUSCLE_INSERT_STRING(key, sym->content->type_name
		    ? sym->content->type_name : "");

		for(int j = 0; j < CODE_PROPS_SIZE; ++j) {
			/* "printer", not "%printer".  */
			char const * pname = code_props_type_string((code_props_type)j) + 1;
			code_props const * p = symbol_code_props_get(sym, (code_props_type)j);
			SET_KEY2("has", pname);
			MUSCLE_INSERT_INT(key, !!p->code);

			if(p->code) {
				SET_KEY2(pname, "file");
				MUSCLE_INSERT_C_STRING(key, p->location.start.file);

				SET_KEY2(pname, "line");
				MUSCLE_INSERT_INT(key, p->location.start.line);

				SET_KEY2(pname, "loc");
				muscle_location_grow(key, p->location);

				SET_KEY(pname);
				obstack_printf(&muscle_obstack,
				    "%*s%s", p->location.start.column - 1, "", p->code);
				muscle_insert(key, obstack_finish0(&muscle_obstack));
			}
		}
#undef SET_KEY2
#undef SET_KEY
	}
}

static void prepare_actions()
{
	/* Figure out the actions for the specified state.  */
	muscle_insert_rule_number_table("defact", yydefact, yydefact[0], 1, nstates);
	/* Figure out what to do after reducing with each rule, depending on
	   the saved state from before the beginning of parsing the data
	   that matched this rule.  */
	muscle_insert_state_number_table("defgoto", yydefgoto, yydefgoto[0], 1, nsyms - ntokens);
	/* Output PACT. */
	muscle_insert_base_table("pact", base, base[0], 1, nstates);
	MUSCLE_INSERT_INT("pact_ninf", base_ninf);
	/* Output PGOTO. */
	muscle_insert_base_table("pgoto", base, base[nstates], nstates + 1, nvectors);
	muscle_insert_base_table("table", table, table[0], 1, high + 1);
	MUSCLE_INSERT_INT("table_ninf", table_ninf);
	muscle_insert_base_table("check", check, check[0], 1, high + 1);
	/* GLR parsing slightly modifies YYTABLE and YYCHECK (and thus
	   YYPACT) so that in states with unresolved conflicts, the default
	   reduction is not used in the conflicted entries, so that there is
	   a place to put a conflict pointer.

	   This means that YYCONFLP and YYCONFL are nonsense for a non-GLR
	   parser, so we could avoid accidents by not writing them out in
	   that case.  Nevertheless, it seems even better to be able to use
	   the GLR skeletons even without the non-deterministic tables.  */
	muscle_insert_int_table("conflict_list_heads", conflict_table, conflict_table[0], 1, high + 1);
	muscle_insert_int_table("conflicting_rules", conflict_list, 0, 1, conflict_list_cnt);
}

/*--------------------------------------------.
| Output the definitions of all the muscles.  |
   `--------------------------------------------*/

static void muscles_output(FILE * out)
{
	fputs("m4_init()\n", out);
	merger_output(out);
	symbol_numbers_output(out);
	type_names_output(out);
	user_actions_output(out);
	/* Must be last.  */
	muscles_m4_output(out);
}
//
// Call the skeleton parser
//
static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
/* Generate a temporary file name based on TMPL.  TMPL must match the
   rules for mk[s]temp (i.e. end in "XXXXXX").  The name constructed
   does not exist at the time of the call to mkstemp.  TMPL is
   overwritten with the result.  */
static FILE* mkstempFILE(char * tmpl, const char * mode)
{
	int len;
	char * XXXXXX;
	static uint64 value;
	uint64 random_time_bits;
	uint count;
	FILE * fd = NULL;
	int r;
	/* A lower bound on the number of temporary files to attempt to
	   generate.  The maximum total number of temporary file names that
	   can exist for a given template is 62**6.  It should never be
	   necessary to try all these combinations.  Instead if a reasonable
	   number of names is tried (we define reasonable as 62**3) fail to
	   give the system administrator the chance to remove the problems.  */
#define ATTEMPTS_MIN (62 * 62 * 62)

	/* The number of times to attempt to generate a temporary file.  To
	   conform to POSIX, this must be no smaller than TMP_MAX.  */
#if ATTEMPTS_MIN < TMP_MAX
	uint attempts = TMP_MAX;
#else
	uint attempts = ATTEMPTS_MIN;
#endif
	len = strlen(tmpl);
	if(len < 6 || strcmp(&tmpl[len - 6], "XXXXXX")) {
		return NULL;
	}
	/* This is where the Xs start.  */
	XXXXXX = &tmpl[len - 6];
	/* Get some more or less random data but unique per process */
	{
		static uint64 g_value;
		g_value = _getpid();
		g_value += 100;
		random_time_bits = (((uint64)234546 << 32) | (uint64)g_value);
	}
	value += random_time_bits ^ (uint64)122434;
	for(count = 0; count < attempts; value += 7777, ++count) {
		uint64 v = value;
		/* Fill in the random bits.  */
		XXXXXX[0] = letters[v % 62];
		v /= 62;
		XXXXXX[1] = letters[v % 62];
		v /= 62;
		XXXXXX[2] = letters[v % 62];
		v /= 62;
		XXXXXX[3] = letters[v % 62];
		v /= 62;
		XXXXXX[4] = letters[v % 62];
		v /= 62;
		XXXXXX[5] = letters[v % 62];

		/* file doesn't exist */
		if(r = _access(tmpl, 0) == -1) {
			fd = fopen(tmpl, mode);
			if(fd) {
				return fd;
			}
		}
	}

	/* We got out of the loop because we ran out of combinations to try.  */
	return NULL;
}

extern int main_m4(int argc, char * const * argv, FILE* in, FILE* out);

static void output_skeleton()
{
	FILE * m4_in = NULL;
	FILE * m4_out = NULL;
	char m4_in_file_name[] = "~m4_in_temp_file_XXXXXX";
	char m4_out_file_name[] = "~m4_out_temp_file_XXXXXX";
	char const * argv[11];

	/* Compute the names of the package data dir and skeleton files.  */
	char const * m4 = m4path();
	char const * datadir = pkgdatadir();
	char * skeldir = xpath_join(datadir, "skeletons");
	char * m4sugar = xpath_join(datadir, "m4sugar/m4sugar.m4");
	char * m4bison = xpath_join(skeldir, "bison.m4");
	char * traceon = xpath_join(skeldir, "traceon.m4");
	char * skel = (IS_PATH_WITH_DIR(skeleton)
	    ? xstrdup(skeleton)
	    : xpath_join(skeldir, skeleton));

	/* Test whether m4sugar.m4 is readable, to check for proper
	   installation.  A faulty installation can cause deadlock, so a
	   cheap sanity check is worthwhile.  */
	xfclose(xfopen(m4sugar, "r"));

	/* Create an m4 subprocess connected to us via two pipes.  */

//  int filter_fd[2];
//  pid_t pid;
	int i = 0;
	{
//    char const *argv[11];
		argv[i++] = m4;

		/* When POSIXLY_CORRECT is set, GNU M4 1.6 and later disable GNU
		   extensions, which Bison's skeletons depend on.  With older M4,
		   it has no effect.  M4 1.4.12 added a -g/--gnu command-line
		   option to make it explicit that a program wants GNU M4
		   extensions even when POSIXLY_CORRECT is set.

		   See the thread starting at
		   <http://lists.gnu.org/archive/html/bug-bison/2008-07/msg00000.html>
		   for details.  */
		//   if (*M4_GNU_OPTION)
		//     argv[i++] = M4_GNU_OPTION;

		argv[i++] = "-I";
		argv[i++] = datadir;
		/* Some future version of GNU M4 (most likely 1.6) may treat the
		   -dV in a position-dependent manner.  See the thread starting at
		   <http://lists.gnu.org/archive/html/bug-bison/2008-07/msg00000.html>
		   for details.  */
		if(trace_flag & trace_m4_early)
			argv[i++] = "-dV";
		argv[i++] = m4sugar;
		argv[i++] = "-";
		argv[i++] = m4bison;
		if(trace_flag & trace_m4)
			argv[i++] = traceon;
		argv[i++] = skel;
		argv[i++] = NULL;
		aver(i <= ARRAY_CARDINALITY(argv));

		if(trace_flag & trace_tools) {
			fputs("running:", stderr);
			for(int j = 0; argv[j]; ++j)
				fprintf(stderr, " %s", argv[j]);
			fputc('\n', stderr);
		}

		/* The ugly cast is because gnulib gets the const-ness wrong.  */
		// pid = create_pipe_bidi ("m4", m4, (char **)(void *)argv, false, true,
		//                         true, filter_fd);
	}
/*
   free (skeldir);
   free (m4sugar);
   free (m4bison);
   free (traceon);
   free (skel);
 */
	if(trace_flag & trace_muscles)
		muscles_output(stderr);
	{
		m4_in = mkstempFILE(m4_in_file_name, "wb+");
		if(!m4_in)
			error(EXIT_FAILURE, get_errno(), "fopen");
		muscles_output(m4_in);
		fflush(m4_in);
		if(fseek(m4_in, 0, SEEK_SET))
			error(EXIT_FAILURE, get_errno(), "fseek");
	}

	/* Read and process m4's output.  */
	timevar_push(tv_m4);
	{
		m4_out = mkstempFILE(m4_out_file_name, "wb+");
		if(!m4_out)
			error(EXIT_FAILURE, get_errno(), "fopen");
	}

	if(main_m4(i-1, (char * const *)argv, m4_in, m4_out))
		error(EXIT_FAILURE, get_errno(), "m4 failed");

	fflush(m4_out);
	if(fseek(m4_out, 0, SEEK_SET))
		error(EXIT_FAILURE, get_errno(),
		    "fseek");
//FILE *in = xfdopen (filter_fd[0], "r");
	scan_skel(m4_out);
	/* scan_skel should have read all of M4's output.  Otherwise, when we
	   close the pipe, we risk letting M4 report a broken-pipe to the
	   Bison user.  */
	aver(feof(m4_out));
	xfclose(m4_in);
	xfclose(m4_out);

	_unlink(m4_in_file_name);
	_unlink(m4_out_file_name);
//  wait_subprocess (pid, "m4", false, false, true, true, NULL);
	timevar_pop(tv_m4);

	SAlloc::F(skeldir);
	SAlloc::F(m4sugar);
	SAlloc::F(m4bison);
	SAlloc::F(traceon);
	SAlloc::F(skel);
}

static void prepare()
{
	/* BISON_USE_PUSH_FOR_PULL is for the test suite and should not be
	   documented for the user.  */
	char const * cp = getenv("BISON_USE_PUSH_FOR_PULL");
	bool use_push_for_pull_flag = cp && *cp && strtol(cp, 0, 10);
	MUSCLE_INSERT_INT("required_version", required_version);
	/* Flags. */
	MUSCLE_INSERT_BOOL("defines_flag", defines_flag);
	MUSCLE_INSERT_BOOL("glr_flag", glr_parser);
	MUSCLE_INSERT_BOOL("nondeterministic_flag", nondeterministic_parser);
	MUSCLE_INSERT_BOOL("synclines_flag", !no_lines_flag);
	MUSCLE_INSERT_BOOL("tag_seen_flag", tag_seen);
	MUSCLE_INSERT_BOOL("token_table_flag", token_table_flag);
	MUSCLE_INSERT_BOOL("use_push_for_pull_flag", use_push_for_pull_flag);
	MUSCLE_INSERT_BOOL("yacc_flag", !location_empty(yacc_loc));

	/* File names.  */
	if(spec_name_prefix)
		MUSCLE_INSERT_STRING("prefix", spec_name_prefix);

	MUSCLE_INSERT_STRING("file_name_all_but_ext", all_but_ext);

#define DEFINE(Name) MUSCLE_INSERT_STRING(#Name, Name ? Name : "")
	DEFINE(dir_prefix);
	DEFINE(mapped_dir_prefix);
	DEFINE(parser_file_name);
	DEFINE(spec_header_file);
	DEFINE(spec_mapped_header_file);
	DEFINE(spec_file_prefix);
	DEFINE(spec_graph_file);
	DEFINE(spec_name_prefix);
	DEFINE(spec_outfile);
	DEFINE(spec_verbose_file);
#undef DEFINE
	/* Find the right skeleton file, and add muscles about the skeletons.  */
	if(skeleton)
		MUSCLE_INSERT_C_STRING("skeleton", skeleton);
	else
		skeleton = language->skeleton;
	/* About the skeletons.  */
	{
		/* b4_skeletonsdir is used inside m4_include in the skeletons, so digraphs
		   would never be expanded.  Hopefully no one has M4-special characters in
		   his Bison installation path.  */
		char * skeldir = xpath_join(pkgdatadir(), "skeletons");
		MUSCLE_INSERT_STRING_RAW("skeletonsdir", skeldir);
		SAlloc::F(skeldir);
	}
}

/*----------------------------------------------------------.
| Output the parsing tables and the parser code to ftable.  |
   `----------------------------------------------------------*/

void output()
{
	obstack_init(&format_obstack);
	prepare_symbols();
	prepare_rules();
	prepare_states();
	prepare_actions();
	prepare_symbol_definitions();
	prepare();
	// Process the selected skeleton file. 
	output_skeleton();
	// If late errors were generated, destroy the generated source files.
	if(complaint_status)
		unlink_generated_sources();
	obstack_free(&format_obstack, NULL);
}
