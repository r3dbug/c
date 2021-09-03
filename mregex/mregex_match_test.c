
#include "mregex.h"

#define TEST_STRING_LENGTH 100
#define TEST_START 0
#define TEST_END 23

char       test[][TEST_STRING_LENGTH] = {       
/*0*/		"b.*b", "aaab1111bb2222bccc", "b1111bb2222b",
/*1*/		"b.*?b", "aaab1111bb2222bccc", "b1111b",
/*2*/		"b.{2,15}b", "aaab1111bb2222bccc", "b1111bb2222b",
/*3*/		"b.{2,15}?b", "aaab1111bb2222bccc", "b1111b",
/*4*/		"b1111b", "aaab1111bb2222bccc", "b1111b",
/*5*/		"b.?", "aaab1111bb2222bccc", "b1",
/*6*/		"b.??", "aaab1111bb2222bccc", "b",
/*7*/		"b.+b", "aaab1111bb2222bccc", "b1111bb2222b",
/*8*/		"b.+?b", "aaab1111bb2222bccc", "b1111b",
/*9*/       "b1+", "aaab1111bb2222bccc", "b1111",
/*10*/		"b1+?", "aaab1111bb2222bccc", "b1",
/*11*/      "(?<=a)b.*b(?=b)", "aaab1111bb2222bccc", "b1111b",         // this one must match!
/*12*/      "(?<=a)b.*?b(?=b)", "aaab1111bb2222bccc", "b1111b",
/*13*/      "b.{0,1}", "aaab1111bb2222bccc", "b1",
/*14*/      "b.{0,1}?", "aaab1111bb2222bccc", "b",
/*15*/      "b.{2,}", "aaab1111bb2222bccc", "b1111bb2222bccc",
/*16*/      "b.{0,0}?", "aaab1111bb2222bccc", "b",  // doesn't make much sense (only for test purposes)
/*17*/      "(?<!a)b.*?b", "aaab1111bb2222bccc", "bb",
/*18*/      "(?<!a)b.*b", "aaab1111bb2222bccc", "bb2222b",
/*19*/      "b.*?b(?!c)", "aaab1111bb2222bccc", "b1111b",
/*20*/      "b.*b(?!c)", "aaab1111bb2222bccc", "b1111bb",
/*21*/      "b.{2}", "aaab1111bb2222bccc", "b11",
/*22*/      "b.{2}?", "aaab1111bb2222bccc", "b11",
/*23*/      "(a+)(.*)(c+)", "aaab1111bb2222bccc", "aaab1111bb2222bccc",
	};

int main(void) {

	char	*pattern, *text, *expected;
	int	errors=0;
	int	t;

	char        result[TEST_STRING_LENGTH];
	signed int  err;
	int i, r;
	char escaped_string[100];

	err = mreg_init();
	if (err) return err;

	if (TEST_END-TEST_START>2) mreg_console_off();

	for (i=TEST_START;i<=TEST_END;i++) {
		pattern=test[i*3];
		text=test[i*3+1];
		expected=test[i*3+2];
		err=mreg_match(pattern, text);
		if (err) {
			mreg_extract(0,text,result);
			t=strcmp(expected,result);
			if (t==0) {
				printf("%d: pattern=%s => passed\n",i,pattern);
			} else {
				errors++;
				printf("%d: Failed %d: pattern=%s text=%s expected=%s calculated=%s\n",i,errors,pattern,text,expected,result);
			}
		} else {
			if (expected[0]==0) {
				/* no match as expected */
				printf("%d: pattern=%s => passed\n",i,pattern);
			} else {
			    errors++;
				printf("%d: Failed %d: pattern=%s text=%s expected=%s error=%d\n", i, errors, pattern, text, expected, err);
			}
		}

	}

	mreg_finish();
}
