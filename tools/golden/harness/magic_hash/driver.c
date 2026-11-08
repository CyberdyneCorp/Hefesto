/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2026 The Hefesto Authors
 *
 * magic-hash-harness: drives Magic's own string hash table (utils/hash.c of the
 * pinned Magic commit, compiled unmodified next to this file) and prints its
 * iteration order. It is the reference tool of golden cases that check
 * requirement "Magic string hash table" (capability hefesto-output-determinism).
 *
 * Input (stdin): the same script that hefesto-cli runs, one command per line:
 *
 *     hefesto::debug::hashorder <requested-size> <op> ...
 *
 * where each <op> is "+key" (HashFind: insert if absent) or "-key" (HashRemove).
 * Blank lines and lines starting with '#' are skipped. Words are separated by
 * blanks; keys contain no blanks, quotes or braces (the cases never need them).
 *
 * Output (stdout): one line per command, the keys in HashStartSearch/HashNext
 * order separated by single spaces. This is the rendering hefesto-cli uses for
 * a list result (see tools/golden/README.md).
 *
 * Build with -fsigned-char so that key bytes are signed on every host, as the
 * requirement prescribes (Magic itself uses the host's plain char).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/magic.h"
#include "utils/hash.h"

#define COMMAND_NAME "hefesto::debug::hashorder"
#define MAX_LINE 65536

/* C99 inline functions of utils/malloc.h need one external definition. */
#include "utils/malloc.h"
extern free_magic1_t freeMagic1_init(void);
extern void freeMagic1(free_magic1_t *m1, void *ptr);
extern void freeMagic1_end(free_magic1_t *m1);

static void print_table(HashTable *table)
{
    HashSearch hs;
    HashEntry *entry;
    const char *sep = "";

    HashStartSearch(&hs);
    while ((entry = HashNext(table, &hs)) != NULL)
    {
        printf("%s%s", sep, entry->h_key.h_name);
        sep = " ";
    }
    printf("\n");
}

static int apply_op(HashTable *table, const char *op)
{
    if (op[0] == '+' && op[1] != '\0')
    {
        HashFind(table, op + 1);
        return 0;
    }
    if (op[0] == '-' && op[1] != '\0')
    {
        HashRemove(table, op + 1);
        return 0;
    }
    fprintf(stderr, "magic-hash-harness: bad op '%s' (expected +key or -key)\n", op);
    return 1;
}

/* Runs one command line; returns 0 on success, 1 on a malformed line. */
static int run_line(char *line)
{
    const char *blanks = " \t\r\n";
    char *word = strtok(line, blanks);
    HashTable table;
    int status = 0;

    if (word == NULL || word[0] == '#') return 0;
    if (strcmp(word, COMMAND_NAME) != 0)
    {
        fprintf(stderr, "magic-hash-harness: unknown command '%s'\n", word);
        return 1;
    }
    word = strtok(NULL, blanks);
    if (word == NULL || atoi(word) <= 0)
    {
        fprintf(stderr, "magic-hash-harness: missing or bad requested size\n");
        return 1;
    }
    HashInit(&table, atoi(word), HT_STRINGKEYS);
    while (status == 0 && (word = strtok(NULL, blanks)) != NULL)
        status = apply_op(&table, word);
    if (status == 0) print_table(&table);
    HashKill(&table);
    return status;
}

int main(void)
{
    static char line[MAX_LINE];

    while (fgets(line, sizeof line, stdin) != NULL)
    {
        if (strchr(line, '\n') == NULL && !feof(stdin))
        {
            fprintf(stderr, "magic-hash-harness: line longer than %d bytes\n", MAX_LINE);
            return 2;
        }
        if (run_line(line) != 0) return 2;
    }
    return 0;
}
