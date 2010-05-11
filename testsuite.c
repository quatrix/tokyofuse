#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tap.h>
#include "utils.h"
#include "tc.h"
#include "metadata.h"

int main(int argc, char **argv)
{
	ok(has_suffix("abcd", "cd"), "abcd ends with cd");
	ok(has_suffix("abcd", "abcd"), "abcd ends with abcd");
	ok(has_suffix("abcd", ""), "everything ends with empty suffixes");
	ok(!has_suffix("abcd", "ccd"), "abcd doesn't end with ccd");
	ok(!has_suffix("abcd", "aabcd"), "suffix longer than string");
	ok(!has_suffix("abcd", "cdb"), "abcd doesn't end with cdb");
	

	char s1[] = "helloworld";
	ok(strcmp(remove_suffix(s1,"world"), "hello") == 0, "helloworld - world = hello");

	char s2[] = "baba";
	ok(remove_suffix(s2,"world") == NULL, "can't remove world from baba");

	char *parent = NULL;

	parent = parent_path("/hey/ho/lets");
	ok(strcmp(parent, "/hey/ho") == 0, "parent of /hey/ho/lets is /hey/ho");

	parent = parent_path("/hey/ho/lets/");
	ok(strcmp(parent, "/hey/ho") == 0, "parent of /hey/ho/lets/ is /hey/ho");

	ok(strcmp(leaf_file(parent), "ho") == 0, "leaf of /hey/ho is ho");

	free(parent);

	char *s;
	s = to_tc_path("/baba/hey");
	
	ok(strcmp(s, "/baba/hey.tc") == 0, "/baba/hey -> /baba/hey.tc");
	
	s = to_tc_path(NULL);
	ok(s == NULL, "null path returns null tc path");



	return EXIT_SUCCESS;
}
