use English;
use strict;


open CONF_FILE, "../libtrash.conf"  or die "Unable to open file ../libtrash.conf.\n";
open HEADER_FILE, ">>trash.h" or die "Unable to open file header file trash.h";

print HEADER_FILE "/* BEGINNING OF AUTOMATICALLY-GENERATED CONFIGURATION SECTION: */\n\n";

while (<CONF_FILE>)
{
    chomp();
    
    next if (0 == index $ARG, "#" || -1 == index $ARG, "="); # Skip comments and lines lacking an equal sign
    
    if (/\s*(\w+)\s*=\s*(.*)\s*/)
    {
	if ($1 eq "DEBUG") # This setting is handled separately because DEBUG = NO means 
	  # "#undef DEBUG" instead of "#define DEBUG NO"
	{
	    print HEADER_FILE "#undef DEBUG\n"  if ($2 eq "NO");
	    print HEADER_FILE "#define DEBUG\n" if ($2 eq "YES");
	}
	elsif ($1 ne "UNCOVER_DIRS") # For settings other than DEBUG and UNCOVER_DIRS
	{
	    print HEADER_FILE "#define $1 "; # Begin macro definition
	    
	    if ($2 eq "YES"     || $2 eq "NO"               ||
		$2 eq "PROTECT" || $2 eq "ALLOW_DESTRUCTION")
	    {
		print HEADER_FILE "$2\n"; # Set macro to pre-defined constant
	    }
	    else
	    {
		print HEADER_FILE "\"$2\"\n"; # Set macro to string
	    }
	    
	}
    }
    
}    

# the more recent python script get_symbol_versions.py will append stuff after genheader.pl is done
# and terminate the automatically generated section of the header file by appending
# "\n/* END OF AUTOMATICALLY-GENERATED CONFIGURATION SECTION */\n" to it

close CONF_FILE;
close HEADER_FILE;

