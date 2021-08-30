
#include "mregex.h"

#define TEST_STRING_LENGTH 100
#define TEST_START 0
#define TEST_END 14

char       test[][TEST_STRING_LENGTH] = {       
/*0*/		"(b.*b)", "aaab1111bb2222bccc", "$1", "b1111bb2222b",
/*1*/		"(b)(.*)(b)", "aaab1111bb2222bccc", "$2", "1111bb2222",
/*2*/		"(b.*?b)", "aaab1111bb2222bccc", "$1", "b1111b",
/*3*/		"(b)(.*?)(b)", "aaab1111bb2222bccc", "$2", "1111",
/*4*/		"(b)(.*?)(b)", "aaab1111bb2222bccc", "\\$2", "$2",
/*5*/		"(b)(.*?)(b)", "aaab1111bb2222bccc", "\\\\$2", "\\1111",
/*6*/		"(?<=a)([^a]{2})", "aaab1111bb2222bccc", "$1", "b1",
/*7*/		"(a+?)(b.*?)(1{1,})(bb)(2+)", "aaab1111bb2222bccc", "#$5#$4#$3#$2#$1", "#2#bb#1#b111#aaa",
// example 7: mixed greediness = _all_ reluctant!!! => do not use mixed patterns!!!
/*8*/		"(a+)([^cC]*)(c+)$", "aaab1111bb2222bccc", "\\U$1\\L$3", "AAAccc",
/*9*/		"(A+)([^cC]*)(C+)", "AAAb1111bb2222bCCC", "\\L$1\\U$3", "aaaCCC",
/*10*/		"([Aa]+)([^cC]*)([cC]+)", "aAab1111bb2222bcCc", "\\U$1\\U$3", "AAACCC",
/*11*/		 "(a+)(.*?)(c+)", "aaab1111bb2222bccc", "\\U$1\\L$3", "AAAc",
// example 11: again: don't use mixed patterns! => $1=greedy, $2 and $3=reluctant ... (= wrong according to PCRE!)
/*12*/		 "(a*)([^c]*)(c{0,3})", "aaab1111bb2222bccc", "\\U$1\\L$3", "AAAccc",
/*13*/		 "(a+)(.*)(c+)", "aaab1111bb2222bccc", "\\U$1$2$3", "AAAb1111bb2222bccc",
/*14*/		 "(?<=.*)(c+)", "aaab1111bb2222bccc", "\\U$1", "CCC",
	};

int main(void) {

	char	*pattern, *text, *expected, *replacement;
	int	    errors=0;
	int	    t;

	char    result[TEST_STRING_LENGTH];

	signed int  err;
	int i;

	err = mreg_init();
	if (err) return err;

	if (TEST_END-TEST_START>2) mreg_console_off();

	for (i=TEST_START;i<=TEST_END;i++) {
		pattern=test[i*4];
		text=test[i*4+1];
		replacement=test[i*4+2];
		expected=test[i*4+3];
		err=mreg_replace(pattern,text,replacement,result);
		if (err==0) {
			t=strcmp(expected,result);
			if (t==0) {
				printf("%d: %s -> %s => passed\n",i,pattern,replacement);
			} else {
				errors++;
				printf("%d: Failed %d: pattern=%s text=%s expected=%s calculated=%s\n",i,errors,pattern,text,expected,result);
			}
		} else {
			if (expected[0]==0) {
				// no match as expected
				printf("%d: pattern=%s => passed\n",i,pattern);
			} else {
			    errors++;
				printf("%d: Failed %d: pattern=%s text=%s expected=%s error=%d\n", i, errors, pattern, text, expected, err);
			}
		}
	}

	mreg_finish();
}
