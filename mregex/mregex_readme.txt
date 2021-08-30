/*
 *     mregex.c / mregex.h
 *
 *     mregex stands for "modified regex", it improves and extends the
 *     functionalities of the Amiga library regex_ranieri.lha written by
 *     Alfonso Ranieri, giving it some (but not a complete set of) features
 *     known as the PCRE (Perl compatible regex expressions) standard.
 *
 *     This program is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU General Public Licens as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     merchantability or fitness for a particular purpose. See the
 *     GNU General Public License for more details.
 *
 *     www.gnu.org/licenses
 *
 *     (c) 2021 RedBug (redbug@gmx.ch)
 *
 *     Date: 30.08.2021
 *     Version: 0.1
 *
 */


INTRODUCTION

The regex_ranieri.lha written by Alfonso Ranieri offers pure POSIX 1003.2
compliant regex functionalities. This means that certain features that may
be quite useful are missing:

- qualifiers for greedy vs reluctant wildcards
- lookaround expressions
- capturing vs non capturing groups

Also, there are other practical functions that are not available:

- replace-function (with variables $1, $2, ...)
- quote-function (to escape search patterns automatically)
- extract-function (to parse text using variables)

mregex adds these functionalities in a simple and straightforward way.

FUNCTIONALITIES

(A) QUALIFIERS FOR GREEDY VS RELUCTANT

In POSIX 1003.2 all wildcards are greedy:

* zero or more occurences
+ one or more occurrences
? zero or one occurrences
{x,y} between x and y occurrences

In the example:

Text:       aaab1111b2222bccc
Pattern:    b.*b

The pattern will match the wider sequence "b1111b2222b". In PCRE the ? char
can be used to restrict the search functionality to the shortest match:

Text:       aaab1111b2222bccc
Pattern:    b.*?b

This pattern matches the sequence "b1111b".

mregex offers reluctant search functionality in the following way:

- it eliminates the ? char from the original pattern
- it then searches for the shortest match using the modified pattern

CAVEATS

(1) DO NOT USE GREEDY AND RELUCTANT QUALIFIERS IN THE SAME PATTERN

Due to the simplified algorithm (elimination of ? and search for the shortest match)
the correct result for patterns with mixed greediness can not be guaranteed.

Text:       aaab1111b2222bb3333bb4444bccc
Pattern:    (b.*?b)(b.*b)(b.*?b)

Correct matches according to PCRE standard are:

Group 1 = "b1111b" (reluctant)
Group 2 = "b2222bb3333" (greedy)
Group 3 = "b4444b" (reluctant)

mregex, however, gives the _shortest_ match:

Group 1 = "b1111b" (reluctant)
Group 2 = "b2222b" (reluctant)
Group 3 = "b3333b" (reluctant)

In other words: Greediness in mregex can only be used globally (on/off for all
groups) and cannot be defined for groups individually.

(B) LOOKAROUND EXPRESSIONS

PCRE defines 2x2=4 types (behind, ahead; positive, negative) of lookaround 
expressions:

(?<=pattern) = positive lookbehind
(?<!pattern) = negative     "
(?=pattern) = positive lookahead
(?!pattern) = negative     "

mregex mimics this functionality by cutting these lookaround expressions out of
the original pattern. It then searches for a match and verifies in an additional
step if the lookaround expressions match (repeating the search if the lookaround
expressions can't be matched in the first attempt).

CAVEATS

(1) ONLY USE LOOKAROUND EXPRESSIONS AT THE BEGINNING/END OF A PATTERN

Due to the "cutting technique" mregex cannot handle lookaround expressions inside
(in the middle) of a pattern.

Pattern:    (?<=before)inside (?<!no)pattern(?=after)

Although perfectly legitimate in PCRE these patterns will NOT work in mregex.

(C) CAPTURING VS NON CAPTURING GROUPS

PCRE knows two types of groups:

(group1) = capturing
(?:group2) = non capturing

Groups are numerated sequentially even if they are inside of other groups:

(group1)(group2)(group3)
(group1(group2(group3))(group4))

In PCRE these numbers can then be referenced by variables inside replace function:

(aaa)(bbb)(ccc) = 3 groups (all marking)

$1 = aaa
$2 = bbb
$3 = ccc

Non captured groups cannot be referenced and are not counted:

(aaa)(?:bbb)(ccc) = 3 groups (1st and 3rd capturing, 2nd non capturing)

$1 = aaa
$2 = bbb

mregex processes non capturing groups by eliminating the ?: marker from the original
pattern. As a consequence, non capturing patterns become capturing and variable 
references may change. To avoid wrong results in the replace function, mregex
automatically keeps track of these changes.

original:   (aaa)(?:bbb)(ccc) => $1=aaa, $2=bbb
becomes:    (aaa)(bbb)(ccc) => $1=aaa, $3=bbb ($2=unused)

In this example mregex automatically "translates" $2 to $3 in the replace function.

CAVEATS

(1) DO NOT EXCEED THE MAXIMUM NUMBER OF $ VARIABLES

Group numbers represented as a single ASCII char after the dolar sign. That's why
mregex cannot handle more than 9 group variables in total. In long expressions having
9 capturing group variables available might seem enough at first glance:

(1)(?:other)(2)(?:non)(3)(?:capturing)(4)(?:groups)(5)(6)(7)(8)(9)

Nontheless, since mregex processes this expression by eliminating ?: from the pattern, 
the total number of capturing groups will increase by 4 (and thus exceed the maximum of 
9 groups mregex can handle).

(1)(other)(2)(non)(3)(capturing)(4)(groups)(5)(6)(7)(8)(9)

This final example contains a total of 13 capturing groups.

(D) REPLACE FUNCTION

As explained in the preceeding section mregex offers a replace function with
variables. In addition, you can use PERL \U and \L syntax to transform groups to upper 
or lower case:

text:           Elefants are big.
pattern:        (^.*)(big)(\.)
replacement:    $1\U$2$3
result:         Elefants are BIG.

Use escaping if you want to use a character with special meaning in regex literally:

\$1 = dollar sign + number 1
$1 = variable 1
\. = dot as a dot
. = regex wildcard for any character

Please note that when writing C programms the compiler needs its own escaping
sequences:

char *string[] = "\\$1"; // means: \$1 = dollar sign + number 1
char *string[] = "\\\\$1"; // means \\$1 = escaped \ + variable 1

So, four \\\\ in C become two \\ for mregex and one (escaped) \ in the final string.

The replace function offers different variants:

- context: mregex copies the entire text into result and replaces the occurrence by
the replacement.
- all: by default, mregex replaces only the first ocurrences; by using the all function,
all occurrencies are replaced.

CAVEAT

The functions mreg_replace_all() and mreg_context_replace_all() may lead to infinite
loops (and eventually corrupt system memory due to buffer overflows). To limit the
risks, a maximum number of iterations can be defined (MREG_ITERATIONS_REPLACE).

(E) QUOTE FUNCTION

Use the mreg_quote(pattern,escaped) function to escape all special characters
automatically.

(F) EXTRACT FUNCTIONS

Use the mreg_extract(group,text,result) function to extract specific groups of 
previously matched patterns.

* * * * * * * * * * *

FUNCTION REFERENCE

int mreg_init(void)
int mreg_finish(void)
signed int mreg_match(char* pattern, char* text)
signed int mreg_replace(char* pattern, char* text, char* replacement, char* result)
signed int mreg_context_replace(char *pattern, char *text, char *replacement, char *result)
signed int mreg_context_replace_all(char *pattern, char *text, char *replacement, char *result)
signed int mreg_extract(int group, char *text, char *result)
int mreg_quote(char *text, char *result)
void mreg_console_on(void)
void mreg_console_off(void)

The other functions are internal helper functions and should not be called directly.

USAGE

Call mreg_init() at the beginning and mreg_finish() at the end of the program. This
opens and closes the Raniery-library and does some necessary initializations.

You can optionally turn the console output off (default) or on. This gives you some
debugging information (e.g. when there is an internal problem like opening the library,
when you a non existing variable in the replace function etc.)

Use mreg_match, mreg_replace and mreg_quote functions without any hassle, just
providing the necessary parameters as strings. Allocating / deallocating memory for
these string will be your business (mregex works with static internal variables
whose sizes can be set at compile time via the define statements).

Have a look at the source code for return values and their exact meaning.

This code was written and compiled using the SAS/C Amiga Compiler version 6.59. Be
aware that you need to install regex_ranieri.lha first (including library AND header
files) to be able to write your own programs.


