
#include "mregex.h"

#define TEST_STRING_LENGTH 100

int main(void) {

	char    text[TEST_STRING_LENGTH] = "Search for a word with seven letters in this text.",
			pattern[TEST_STRING_LENGTH] = "([^ ]{7})",
			replacement[TEST_STRING_LENGTH] = "#######",
			replacement2[TEST_STRING_LENGTH] = "#$1#something#$1#",
			result[TEST_STRING_LENGTH];

	signed int err, ret;

	/* initialize mregex */
	err = mreg_init();
	if (err) return err;

	/* turn console output off */
	mreg_console_off();

	/* test if there is a match */
	ret=mreg_match(pattern,text);
	if (ret>0) {
	    printf("A word with 7 letters was found.\n");
		
		/* extract match = group 1 = $1 */
		ret=mreg_extract(1,text,result);
		if (ret!=0) printf("Error: %s\n", result);
		else printf("The word is: %s\n",result);

		/* replace word in text */
		mreg_context_replace(pattern, text, replacement, result);
		printf("Replacement in entire text: %s\n",result);

		/* build a new string with matching part */
		mreg_replace(pattern, text, replacement2, result);
		printf("Replacement witch match: %s\n", result);

		/* escape a search string */
		mreg_quote(pattern,result);
		printf("Escaping a search string:\n");
		printf("Original: %s\n", pattern);
		printf("Escaped: %s\n", result);

	} else printf("No succes - no word with 7 letters.\n");

	mreg_finish();
}
