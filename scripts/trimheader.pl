use English;
use strict;

open OLD_HEADER, "trash.h" or die "Unable to open current header file trash.h\n";
open NEW_HEADER, ">trash-new.h" or die "Unable to open new header file trash-new.h\n.";

while (<OLD_HEADER>)
{
    
    last if (-1 != index $ARG, "BEGINNING OF AUTOMATICALLY-GENERATED CONFIGURATION SECTION");
    
    print NEW_HEADER "$ARG";
}

close OLD_HEADER;
close NEW_HEADER;

rename "trash-new.h", "trash.h" or die "Unable to rename file trash-new.h to trash.h.\n";
