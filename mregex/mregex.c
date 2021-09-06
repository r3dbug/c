
#include "mregex.h"

// variables

// globals

regmatch_t 	    subs1[MREG_NS], subs2[MREG_NS], tsubs[MREG_NS];
regmatch_t      *subs;
ULONG 		    eopts=0;
int 		    i, len;
regoff_t 	    startoff = 0;
regoff_t 	    endoff = 0;
char            errbuf[256];
regex_t	        re;
int             copts = REG_EXTENDED;

// switches

char            mreg_internal_buffer[MREG_STRING_LENGTH];
char            mreg_lookahead[MREG_STRING_LENGTH];
int             mreg_lookahead_type;
char            mreg_lookbehind[MREG_STRING_LENGTH];
int             mreg_lookbehind_type;
char            mreg_pattern[MREG_STRING_LENGTH];
int             mreg_search_lookahead;
int             mreg_capturing_groups[10];
int             mreg_number_capturing_groups;
int			    mreg_replacement_type;
unsigned int    mreg_use_non_greedy = 0; // when set => calculate reluctant
unsigned int    mreg_leave_on_first=0; // used for "reluctant, but greedy" lookaround search
unsigned int    mreg_no_errors = MREG_NO_ERRORS;
unsigned int    mreg_console = MREG_CONSOLE;
unsigned int    mreg_old_use_non_greedy;

// functions

void mreg_console_on(void) {
	mreg_console=1;
}

void mreg_console_off(void) {
	mreg_console=0;
}

int mreg_is_escaped(char *text, int position) {
	int c, p;
	c=0; p=position;
	while ((text[p]=='\\') && (p>=0)) { c++; p--; }
	return (c & 1);
}

int mreg_quote(char *text, char *result) {
	int s,d;
	const char escape_list[]=".\\+*?[^]$(){}=!<>:-#";

	s=0; d=0;

	while (text[s]!=0) {
		if (strchr(escape_list,text[s])) result[d++]='\\';
		result[d++]=text[s++];
	}
	result[d++]=0;
	return d;
}

int mreg_eliminate_non_capturing_groups(char *pattern) {
	int s, d, g1, g2;
	s=0; d=0; g1=0; g2=0;

	mreg_capturing_groups[0]=0;

	while (pattern[s]!=0) {
		switch (pattern[s])	{
			case '(' :  if ((pattern[s+1]=='?') && (pattern[s+2]==':')) {
							pattern[d++]=pattern[s++];
							s+=2;
							g1++;
							g2+=2;
							mreg_capturing_groups[g1]=g2;
						} else {
							pattern[d++]=pattern[s++];
							g1++;
							g2++;
							mreg_capturing_groups[g1]=g2;
						}
						break;
			default : pattern[d++]=pattern[s++];
		}
	}
	pattern[d++]=0;
	mreg_number_capturing_groups=g1;
	return g2;
}

signed int mreg_eliminate_lookbehind(char *pattern, char *lookbehind) {
	int s, d, r, stop;
	d=0, stop=0, r=0;
	lookbehind[0]=0;
	if ((pattern[0]=='(') && (pattern[1]=='?')) {
		if (pattern[2]=='<') {
			if (pattern[3]=='=') r=1; // positive lookbehind
			else if (pattern[3]=='!') r=-1; // negative lookbehind
			else r=MREG_LOOKBEHIND_ERROR; // bad format
			/* pattern => lookbehind */
			if (r==MREG_LOOKBEHIND_ERROR) s=3; else s=4; // accept (?<..) as (?<=..)
			while (!stop) {
				switch (pattern[s]) {
					case ')' :  if (mreg_is_escaped(pattern, s-1))
					                lookbehind[d++]=pattern[s++];
								else stop=1;
								break;
					default : lookbehind[d++]=pattern[s++];
				}
			}
			lookbehind[d]='$'; // add $ anchor
			lookbehind[d+1]=0;
			/* shift left pattern */
			s=4+d+1; d=0;
			while (pattern[s]!=0) pattern[d++]=pattern[s++];
			pattern[d]=0;
		} else r=0;
	}
	return r;
}

signed int mreg_eliminate_lookahead(char *pattern, char *lookahead) {
	int s,d,r,stop,c1,c2;
	s=strlen(pattern)-1;
	stop=0; d=1;
	lookahead[0]='^'; // add ^ anchor
	while ((!stop) && (s>=2)) {
		if (pattern[s]=='?') {
		    c1=pattern[s-1];
		    c2=pattern[s+1];
			if ((c1=='(') && ((c2=='=') || (c2=='!')))
				if (!mreg_is_escaped(pattern,s-2)) stop=1;
		}
		s--;
	}
    if (stop) {
		if (c2=='=') r=1;
		else r=-1;
		/* shorten pattern string */
		pattern[s]=0;
		/* pattern => lookahead */
		s=s+3;
		while (pattern[s]!=0) lookahead[d++]=pattern[s++];
	} else r=0;
	lookahead[--d]=0; // overwrite )
	return r;
}

void mreg_eliminate_lookaround(char *pattern, char* lookbehind, char *lookahead)
{
	mreg_lookbehind_type=mreg_eliminate_lookbehind(pattern,lookbehind);
	if (mreg_console) printf("Lookbehind: type: %d pattern: %s\n",mreg_lookbehind_type,mreg_lookbehind);
	mreg_lookahead_type=mreg_eliminate_lookahead(pattern,lookahead);
	if (mreg_console) printf("Lookahead: type: %d pattern: %s\n",mreg_lookahead_type,mreg_lookahead);
}

int mreg_eliminate_nongreedy(char* pattern) {
	int s, d, m;
	char p1, p2, p3;
	s = 0; d=0; m=0;
	while (pattern[s] != 0) {
		switch (pattern[s]) {
			case '?' :  if (s==0) p1=' '; else p1=pattern[s-1];
						if (s<=1) p2=' '; else p2=pattern[s-1];
						if (s<=2) p3=' '; else p3=pattern[s-2];
						//if (strchr(p1,wildcards)) {
						if ((p1=='*') || (p1=='+') || (p1=='?') || (p1=='}')) {
							switch (p2) {
								case '\\' :  if (p3=='\\') { s++; m++; } // don't copy
											break;
								default : s++; m++; break; // nongreedy marker => don't copy
							}
						} else pattern[d++]=pattern[s++];
						break;
			default :
						pattern[d++]=pattern[s++];
		}
	}
	pattern[d]= 0;
	if (m) mreg_use_non_greedy = 1; // set non-greedy switch
	return m;
}

int mreg_build_replace(regmatch_t* subs, char* replacement, char* text, char* result) {
	int s=0, d=0, n, t, r;
	int conversion;

	conversion=MREG_NO_CONVERSION;

	if (mreg_console) printf("Context replacement: type=%d\n", mreg_replacement_type);
	if (mreg_replacement_type==MREG_CONTEXT_REPLACEMENT) {
		strncpy(result,text,subs[0].rm_so);
		d=subs[0].rm_so;
	}

    while (replacement[s] != 0) {
        switch (replacement[s]) {
			case '$' :	if (mreg_is_escaped(replacement, s-1)) result[d++]=replacement[s++];
						else if (replacement[s+1]==0) result[d++]=replacement[s++];
						else {
							r = replacement[++s]; n=r-'0';
							if ((n >= 0) && (n <= 9)) {
								if (n>mreg_number_capturing_groups) {
									/* ignore non existing groups */
									if (mreg_console) printf("invalid capturing group $%d in replacement (ignored).\n",n);
								} else {
								    n=mreg_capturing_groups[n]; // take care of non capturing groups
								    for (t=subs[n].rm_so; t<subs[n].rm_eo; t++) {
									    switch (conversion) {
										    case MREG_CONVERT_TO_UPPER : result[d++] = toupper(text[t]); break;
										    case MREG_CONVERT_TO_LOWER : result[d++] = tolower(text[t]); break;
										    default : result[d++] = text[t];
									    }
								    }
								    conversion=MREG_NO_CONVERSION;
								}
								s++;
							} else result[d++]=replacement[s++];
						 }
                         break;
			case '\\' :  switch (replacement[s+1]) {
							case '\\' : result[d++]=replacement[s++]; s++; break;
							case 'U' : conversion=MREG_CONVERT_TO_UPPER; s+=2; break;
							case 'L' : conversion=MREG_CONVERT_TO_LOWER; s+=2; break;
							default : s++;
						 }
						 break;
			default :
						conversion=MREG_NO_CONVERSION;
						result[d++] = replacement[s++];
        }
    }

	if (mreg_replacement_type==MREG_CONTEXT_REPLACEMENT) {
		s=subs[0].rm_eo;
		while (text[s]!=0) result[d++]=text[s++];
	}
    result[d++] = 0;
    return 0;
}

regmatch_t* mreg_match_shorter(regex_t re, char* text, regoff_t start, regoff_t end) {
	int li, mi, hi, fix, d1, d2, initial_eo, leave_on_first; // low/middle/high interval
    char temp;
	signed int err;
	regmatch_t *asubs, *match, *t;
	li=start; hi=end; fix=li;
	asubs = subs2;
	match = subs1;
	mi=hi;//(li+hi)/2;
	d1=99999;
	leave_on_first=0;
	initial_eo=match[0].rm_eo;
		
	if (mreg_console) printf("Search for shorter in: %s\n",text);
	do {
		d2=d1;
		temp=text[mi+1]; text[mi+1]=0;
		err = regexec(&re,text,(size_t)MREG_NS,asubs,eopts);
		if (mreg_console) printf("Test[%d-%d]: li=%d, mi=%d, hi=%d; err=%d; str=%s\n", fix, mi, li, mi, hi, err, text);
		text[mi+1]=temp;
		if (err) {
			if (mreg_leave_on_first && (mreg_old_use_non_greedy==0)) {
				// no match
				// try with string minus last character (brute force)
				hi-=1;
				mi=hi;
				if (mreg_console) printf("Brute force: li=%d mi=%d hi=%d\n", li, mi, hi);
			} else { 
				li=mi;
				mi=(li+hi)/2; // no match => bigger interval
				if (mreg_console) printf("no match\n");
			}
		} else {
			if (mreg_console) printf("mreg_leave_on_first=%d mreg_use_non_greedy=%d\n", mreg_leave_on_first, mreg_use_non_greedy);
			if (mreg_leave_on_first && (mreg_old_use_non_greedy==0)) {
				hi-=1;
				mi=hi;
				if (mreg_console) printf("Brute force: li=%d mi=%d hi=%d\n", li, mi, hi);
			} else {
				hi=mi;
				mi=(li+hi)/2;
			}
			t=match; match=asubs; asubs=t;
			if (initial_eo!=match[0].rm_eo) {
				if (mreg_console) {	
					printf("=> VALID MATCH[%d-%d]: %.*s\n", asubs[0].rm_so, asubs[0].rm_eo, asubs[0].rm_eo-asubs[0].rm_so, text+asubs[0].rm_so);
					printf("CHECK LOOKAROUNDS:\n");
			   		//printf("lb=%s type=%d position=%d text=%s\n",mreg_lookbehind,mreg_lookbehind_type, asubs[0].rm_so, text, text); 
			   		//printf("la=%s type=%d position=%d text=%s\n",mreg_lookahead,mreg_lookahead_type, asubs[0].rm_eo, text);
				}
			 	temp=mreg_check_lookaround(mreg_lookbehind,mreg_lookbehind_type, asubs[0].rm_so, text, asubs[0].rm_eo, mreg_lookahead_type, mreg_lookahead); 
				if (temp && mreg_leave_on_first) {
					leave_on_first=1;
					match=asubs;
			   		if (mreg_console) {	
						printf("GREEDY MATCH FOUND IN SEARCH FOR SHORTER\n");
						printf("leave_on_first=%d\n", mreg_use_non_greedy, leave_on_first);
						printf("result: so=%d eo=%d text=%s\n",asubs[0].rm_so,asubs[0].rm_eo,text);
					}
				} else if (!temp && mreg_console) printf("NO MATCH FOR LOOKAROUNDS => continue search for shorter\n");
			} else { 
				if (mreg_console) {
					printf("=> DISCARD THIS MATCH[%d-%d]: %.*s\n", asubs[0].rm_so, asubs[0].rm_eo, asubs[0].rm_eo-asubs[0].rm_so, text+asubs[0].rm_so);
				}
			}
		}
		d1=hi-li;
	} while ((d1>=0) && (d1!=d2) && (!leave_on_first));
	return match;
}

signed int mreg_check_lookbehind(char* lookbehind, int type, int p, char *text) {
	regmatch_t  *la_subs;
	regex_t	la_re;
	char c; signed int r;
	signed int err;

	la_subs=tsubs;
	c=text[p]; text[p]=0; // end string here

	if (mreg_console) printf("Check lookbehind: string: %s type: %d position: %d text: %s\n", lookbehind, type, p,
    text);
	err=regcomp(&la_re,lookbehind,copts);
	err=regexec(&la_re,text-mreg_search_lookahead,(size_t)MREG_NS,la_subs,eopts); // sub 1 because of try again (start = pos+1)
	text[p]=c; // repair string

	if (err) {
		if (mreg_console) printf("no match => check type: %i\n",type);
		if ((err==1) && (type<0)) r=1;
		else r=0;
	} else {
		if (mreg_console) printf("match => check type: %i\n",type);
		if (type>0) r=1;
		else r=0;
	}
	return r;
}

signed int mreg_check_lookahead(char* lookahead, int type, int p, char *text_arg) {
	regmatch_t  *la_subs;
	regex_t	la_re;
	signed int r;
	char *text;
	signed int err;

	la_subs=tsubs;
	text=text_arg+p; // point to start

	if (mreg_console) printf("Check lookahead: string: %s type: %d position: %d text: %s\n", lookahead, type, p, text);
	err=regcomp(&la_re,lookahead,copts);
	err=regexec(&la_re,text,(size_t)MREG_NS,la_subs,eopts);

	if (err) {
		if (mreg_console) printf("no match => check type: %i\n",type);
		if ((err==1) && (type<0)) r=1;
		else r=0;
	} else {
		if (mreg_console) printf("match => check type: %i\n",type);
		if (type>0) r=1;
		else r=0;
	}
	return r;
}

signed int mreg_check_lookaround(char *lookbehind, int t1, int p1, char* text, int p2, int t2, char* lookahead) {
	int rlb, rla;
	rla=1; rlb=1;
	if (mreg_lookbehind[0]!=0) {
		rlb=mreg_check_lookbehind(lookbehind, t1, p1, text);
    }
    if (mreg_lookahead[0]!=0) {
		rla=mreg_check_lookahead(lookahead, t2, p2, text);
    }
	if (mreg_console) printf("Result lookarounds: lb: %d la: %d\n", rlb, rla);
	return (rlb && rla);
}

signed int mreg_replace(char* pattern_arg, char* text_arg, char* replacement, char* result) {
	/* replacement == NULL => only check if pattern matches
	 * result != null => write error string into result (see MREG_NO_ERRORS)
	 */

	int err,t,d;
	char *text;
	char *pattern;

	pattern=mreg_internal_buffer;
	text=text_arg;
	mreg_search_lookahead=0;
	mreg_use_non_greedy=0; // start in default = greedy mode
	mreg_leave_on_first=0;

	strcpy(mreg_internal_buffer,pattern_arg); // don't trash callers pattern (make copy)

	err=mreg_eliminate_nongreedy(pattern);
	if (err) if (mreg_console) printf("Nongreedy: %s\n",pattern);

	mreg_eliminate_lookaround(pattern,mreg_lookbehind,mreg_lookahead);

    err=mreg_eliminate_non_capturing_groups(pattern);
	if (mreg_console) {
		printf("Capturing groups: g2=%d (number=%d)\n",err,mreg_number_capturing_groups);
	    printf("Pattern: %s\n",pattern);
		for (t=0;t<10;t++) printf("%d=>%d ",t,mreg_capturing_groups[t]);
		printf("\n");
	}
	
	subs[0].rm_so = startoff;
	subs[0].rm_eo = strlen(text) - endoff;

again:
	err = regcomp(&re,pattern,copts);
	if (err) {
		regerror(err,&re,errbuf,sizeof(errbuf));
		if (mreg_console) printf("error in pattern: %s\nregcomp: %s\n",pattern,errbuf);
		if (replacement==MREG_MATCH_MODE) {
			if (result!=MREG_NO_ERRORS) strcpy(result, errbuf);
		    return 0; // 0 == doesn't match
		} else return err;
	}

	err = regexec(&re,text,(size_t)MREG_NS,subs,eopts);
	if (err) {
		regerror(err,&re,errbuf,sizeof(errbuf));
		if (mreg_console) printf("regexec: %s\n",errbuf);
		regfree(&re);
		
		if ((err==1) && ((mreg_lookahead_type!=0) && (mreg_use_non_greedy==0))) {
		    mreg_search_lookahead=0;
		    mreg_leave_on_first= mreg_use_non_greedy ? 0 : 1;
			if (mreg_console) printf("ACTIVATE SHORTER SEARCH FOR LOOKAROUNDS\nmreg_leave_on_first=%d mreg_use_non_greedy=%d\n",mreg_leave_on_first, mreg_use_non_greedy);
			mreg_old_use_non_greedy=mreg_use_non_greedy;
			mreg_use_non_greedy=1;
		    text=text_arg;
		    goto again; // yes: goto! (again!:) - example: (?<a)b.*b(?=b) in aaab1111bb2222bccc => try non greedy
		}

		if (replacement==MREG_MATCH_MODE) {
            if (result!=MREG_NO_ERRORS) strcpy(result, errbuf);
			return 0; // 0 == doesn't match
		} else return err;
	}
	if (mreg_use_non_greedy) subs=mreg_match_shorter(re, text,
	subs[0].rm_so, subs[0].rm_eo);

	/* check lookaround */
	err=mreg_check_lookaround(mreg_lookbehind, mreg_lookbehind_type, subs[0].rm_so, text, subs[0].rm_eo,
	mreg_lookahead_type, mreg_lookahead);
	if (err) {
		d=text-text_arg;
		if (mreg_console) {
		    printf("We have a full match!\n");
			printf("so=%d eo=%d text-text_arg=%d\n", subs[0].rm_so, subs[0].rm_eo, d);
		}
		subs[0].rm_so += d;
		subs[0].rm_eo += d;
	} else if (err==0) {
		mreg_search_lookahead=1;
		text=text+subs[0].rm_so+1;
		if (mreg_console) printf("Try again with position %d (string: %s)\n",subs[0].rm_so+1,text);
		goto again;  // yes: goto!
	}

	/* match mode => return length of match */
	if (replacement==MREG_MATCH_MODE) {
        if (result!=MREG_NO_ERRORS) strcpy(result, errbuf);
		regfree(&re);
		return (subs[0].rm_eo-subs[0].rm_so);
	}

	/* replace mode => build replacement */
	mreg_build_replace(subs, replacement, text, result);

    regfree(&re);
	return 0; // 0 == all ok
}

signed int mreg_replace_all(char *pattern, char *text, char *replacement, char *result) {
	int r,i,hits;
	hits=0;
	if (mreg_replacement_type==MREG_PATTERN_REPLACEMENT) {
		r=mreg_replace(pattern, text, replacement, result);
		if (r==0) hits++;
	} else {
		i=0;
		do {
			r=mreg_replace(pattern, text, replacement, result);
			if (mreg_console) printf("r=%d\n",r);
			strcpy(text,result);
			if (r==0) hits++;
			i++;
		} while ((r==0) && (i<MREG_ITERATIONS_REPLACE));
	}
	return hits;
}

signed int mreg_context_replace(char *pattern, char *text, char *replacement, char *result) {
	int r;
	mreg_replacement_type=MREG_CONTEXT_REPLACEMENT;
	r=mreg_replace(pattern, text, replacement, result);
	mreg_replacement_type=MREG_PATTERN_REPLACEMENT;
	return r;
}

signed int mreg_context_replace_all(char *pattern, char* text, char *replacement, char *result) {
	int r;
	mreg_replacement_type=MREG_CONTEXT_REPLACEMENT;
	r=mreg_replace_all(pattern, text, replacement, result),
	mreg_replacement_type=MREG_PATTERN_REPLACEMENT;
	return r;
}

signed int mreg_match(char* pattern, char* text) {
	return mreg_replace(pattern, text, MREG_MATCH_MODE, MREG_NO_ERRORS);
}

int mreg_extract(int group, char* text, char *result) {
	/* use this function immediately after invoking mreg_replace() or mreg_match() */
	int so,eo,len;
	if (group>mreg_number_capturing_groups) {
		strcpy(result,"Invalid group number");
		return 1; // error
	} else {
		so=subs[group].rm_so;
		eo=subs[group].rm_eo;
		len=eo-so;
		if (mreg_console) printf("Extract: group=%d so=%d eo=%d text=%s\n", group, subs[group].rm_so, subs[group].rm_eo, text);
		strncpy(result, text+so,len);
		result[len]=0;
		return 0;
	}
}

int mreg_init(void) {
	subs=subs1;
	RegexBase = OpenLibrary("regex.library",0);
	if (!RegexBase) {
		if (mreg_console) printf("no RegexBase\n");
		return 1;
	} else return 0;
}

int mreg_finish(void) {
	CloseLibrary(RegexBase);
	return 0;
}

