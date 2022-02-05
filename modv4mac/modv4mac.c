
/* MODV4MAC - modify MAC-address on v4net.device network driver
   (c) 2022 RedBug

   This program is free software: you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free Software
   Foundation, either version 3 of the License, or (at your option) any later 
   version.
   
   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
   Link: http://www.gnu.org/licenses
   
 */

#include <proto/dos.h>
#include <exec/exec.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISCLAIMER "04/02/2022 by RedBug (feel free to use and modify this program:)\n"
#define MAC_SHOW 1
#define MAC_PATCH 2
#define MODE_ERROR 3

UBYTE   orig[] = { 0x06, 0x80, 0x11, 0x04, 0x04, 0x04 };        // default mac address
UBYTE   mac[] = { 0x06, 0x80, 0x11, 0x04, 0x04, 0x04 };         // new mac address (initialize with default)

FILE*   fh;
char    filename[]="v4net.patched";
UBYTE*  buffer;
ULONG   size;
ULONG   csize;                                                  // length of comparison (5 for repatch or 6 for patch)
ULONG   mode;

int ParseMAC(char* s) {
    char c;
    int i=0, n=5, j;
    while (s[i++]!=0) if (s[i]==':') n--; 
    if (n<0) return 1;
    i=0;
    while (s[i]!=0) {
        j=i;
        while ((s[j]!=':') && (s[j]!=0)) j++;
        c=s[j]; s[j]=0;
        mac[n]=strtol(&s[i], NULL, 16);
        // printf ("%d: s[%d-%d]=%s byte=%x\n",n, i, j, &s[i],mac[n]);
        n++;
        i = c ? j+1 : j;
    }
    return 0;
}

void PatchDriver(char* s) {     
    ULONG t,p,r,l;
    
    printf("Filename: %s\n",s);
       
    // open file
    fh=fopen(s,"r");
    if (fh==NULL) return;
    
    // get file size
    fseek(fh,0,2);
    size=ftell(fh);
    if (size==0) return;

    // allocate buffer
    buffer = AllocMem(size * sizeof(UBYTE), MEMF_ANY);
    if (buffer==NULL) return;
    
    // read file into buffer & close
    rewind(fh);
    t=fread(buffer,1, size, fh);
    if (t!=size) printf("Error: file size: %d read: %d\n",size,t);
    fclose(fh);

    // search default mac-address
    r=1; p=size-6;
    while ((p>=0) && (r!=0)) {
        r=memcmp((void const*) &buffer[p],(void const*)&orig[0],csize);
        if (r==0) {
            if (mode!=MAC_SHOW) {
                printf("Match found at offset: [%x]\n", p);
                printf("Current MAC-address: ");
                for (l=0;l<6;l++) {
                    printf("%02x",buffer[p+l]);
                    if (l!=5) printf(":");
                }
                printf("\n");
            }
        } else p--;
    }

    // show actual MAC-address (mode == MAC_SHOW)
    if (mode==MAC_SHOW) {
        printf("Actual MAC-address: ");
        for (l=0;l<6;l++) {
            printf("%02x",buffer[p+l]);
            if (l!=5) printf(":");
        }
        printf("\n");
        FreeMem(buffer,sizeof(UBYTE)*size);
        return;    
    }
    
    // replace with new mac-address
    if (r==0) {
        printf("Replace with new MAC: ");
        for (l=0;l<6;l++) {
            buffer[p+l]=mac[l];
        }
        for (l=0;l<6;l++) {
            printf("%02x",buffer[p+l]);
            if (l!=5) printf(":");
        }
        printf("\n");
    } else {
        printf("No match found (try repatch instead of patch).\n");
        FreeMem(buffer, sizeof(UBYTE)*size);
        return;
    }
    
    // write patched driver to file
    fh=fopen(filename,"w");
    if (fh==NULL) return;
    t=fwrite(buffer,1, size, fh);
    if (t!=size) printf("Error: file size: %d read: %d\n",size,t);
    fclose(fh);
            
    //printf("size=%d t=%d\n",size,t);
    
    printf("Patched file: %s\n",filename);
    
    // free memory
    FreeMem(buffer, sizeof(UBYTE)*size);
}

ULONG SetOperationMode(char* s) {
    if (strcmp("patch",s)==NULL) {
        csize=6;
        mode=MAC_PATCH;
    } else if (strcmp("repatch",s)==NULL) {
        csize=3; // repatch = only compare 5 hex numbers (last number can be remodified)
        mode=MAC_PATCH;
    } else if (strcmp("show",s)==NULL) {
        csize=3;
        mode=MAC_SHOW; 
    } else if (strcmp("commodore",s)==NULL) {
        mac[0]=0x00;
        mac[1]=0x80;
        mac[2]=0x10;
        csize=3;
        mode=MAC_PATCH;
    } else mode=MODE_ERROR;
    return mode;
}


int main(int argc, char* argv[]) {
    
    char* s;
    
    switch (argc) {
        case 3 :
        case 4 : // printf("arg1=%s arg2=%s arg3=%s\n",argv[1], argv[2], argv[3]);
                 if (SetOperationMode(argv[1])==MODE_ERROR) {
                    printf("Incorrect operation mode '%s' (argument 1 must be: patch|repatch|show)\n",argv[1]);
                    return 1;
                 }
                 switch (mode) {
                    case MAC_PATCH : if (argc!=4) { 
                                        printf("Wrong number of arguments.\n");
                                        return 1;
                                     }
                                     break;
                    case MAC_SHOW  : if (argc!=3) {
                                        printf("Wrong number of arguments.\n");
                                        return 1;
                                     }
                                     break;
                 }
                 if (mode!=MAC_SHOW) {
                    if (ParseMAC(argv[2])) {
                       printf("Error parsing string.\n");
                        return 1;
                    }
                 }
                 s = (mode==MAC_SHOW) ? argv[2] : argv[3];
                 PatchDriver(s);
                 break;
             
        default :
                 printf("MODV4MAC - modify MAC-address of V4 network driver\n");
                 printf(DISCLAIMER);
                 printf("USAGE 1 (patch):\n");
                 printf("1) cd DEVS:networks\n");
                 printf("2) copy v4net.device v4net.original\n");
                 printf("3) modv4mac patch 05 v4net.device\n");
                 printf("4) protect v4net.device +e\n");
                 printf("5) copy v4net.patched v4net.device\n");
                 printf("6) reboot\n");
                 printf("=> changes default 06:80:11:04:04:04 to 06:80:11:04:04:05\n");
                 printf("=> use modv4mac patch 05:06:07 (or more numbers) to change last 3 (or more) hex numbers\n");
                 printf("USAGE 2 (repatch):\n");
                 printf("1) modv4mod repatch 06 v4net.patched\n");
                 printf("=> changes patched 06:80:11:04:04:05 to 06:80:11:04:04:06\n");
                 printf("=> repatching only works if you change only one (= last) hex number!\n");
                 printf("USAGE 3 (commodore):\n");
                 printf("1) modv4mac commodore 06 v4net.device\n");
                 printf("=> changes default 06:80:11:04:04:04 to 00:80:10:04:04:06\n");
                 printf("=> 00:80:10 is the official Commodore OUI (= first 3 hex numbers)\n");
                 printf("=> change only last 3 hex numbers (otherwhise you will overwrite Commodore OUI)\n");
                 printf("USAGE 4 (show):\n");
                 printf("1) modv4mac show v4net.patched\n");
                 printf("=> shows 06:80:11:04:04:xx (= MAC-address of patched driver)\n");
                 printf("=> works only if you change only one (= last) hexadecimal number\n");
                 printf("Always keep a copy of your original driver!\n");
                 printf("This program scans for default address in v4net.device (06:80:11:04:04:04)\n");
                 printf("It might not work with future versions of the driver.\n");
                 printf("This program is Free Software under the GPL.\n");
                 printf("Use it at your own risk - I am not responsible for any damage done to your system!\n");
                 break;
        
    }

}