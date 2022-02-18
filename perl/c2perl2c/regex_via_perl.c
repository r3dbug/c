

/*

    regex_via_perl.c
    
    Reads a string from console and sends it to a perl script
    that acts as a server.
    
    data_via_ram_file.pl
    
    PERL script that acts as a server and performs a REGEX on
    string sent from C-program via RAM-disk.
    
    The data is exchange via AmigaOS RAM-disk:
    
    INPUT:  ram:test.txt
    OUT:    ram:result.txt
    
    Both programs have to wait for the other to finish:
    
    PERL: Waits using the (-e "RAM:$filename") expression.
          In addition it goes to sleep (e.g. 100 ms) in order
          consume less cpu ressources.
          
    C: Uses the delay() function.
    
    Could be optimized in different ways:
    
    - C-program doesn't use a fix delay (but checks if file exists,
      like PERL)
    - Both programs uses system messages to notify each other about
      request / data sent.

    To run the programs:
    
    - first start PERL-scrit in a shell (perl data_via_ram_file.pl)
    - the start C-program in another shell (or use run for the first 
      step to use the same shell)
      
    Has been tested with:
    
    - PERL 5.7.1
    - SAS/C 6.59

*/

#include <stdio.h>
#include <proto/dos.h>

extern DOSBase;

int main(void) {

    char    text[200];
    FILE*   fh;
    
    char    perl_in[] = "ram:test.txt";
    char    perl_out[] = "ram:result.txt";
    char    result_old[] = "ram:result.old";
   
    text[0]=0;
    
    do {
        printf("Text: ");
        gets(text);
        
        fh=fopen(perl_in,"w");
        if (!fh) return 1;
        
        fputs(text,fh);
        fclose(fh);
        
        // wait to give time to perl server to process regex and write file
        Delay(10);
        
        // read result
        fh=fopen(perl_out,"r");
        if (!fh) return 1;
        
        while (!feof(fh)) {
            fgets(text,199,fh);
            printf("Text: %s\n", text); 
        }
        fclose(fh);
        
        // clean up files
        remove(result_old);
        rename(perl_out, result_old);
    
    } while (text[0]!=0);
    return 0;
}
