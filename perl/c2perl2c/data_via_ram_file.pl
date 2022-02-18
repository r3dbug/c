#!/usr/bin/perl

$filename="RAM:test.txt";
$result="RAM:result.txt";

while (1) {
    # sleep(1);
    select(undef,undef,undef,0.1); # sleep 100 ms
    if (-e "RAM:$filename") {
        open(DATAIN,"<$filename") or die("couldn't open $filename!");
        my @lines=<DATAIN>;
        close(DATAIN);
        open(DATAOUT,">$result") or die("couldn't create $result!\n");
        foreach $element (@lines) {
            $element =~ s/^(.*)$/\U$1/;
        }
        print DATAOUT @lines;
        close(DATAOUT);
        # delete file
        unlink("$filename");
    }
}