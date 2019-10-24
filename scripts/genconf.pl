use English;
use strict;


sub write_info
{
    my $personal_conf_file = shift();
    
    print OUTFILE "# THIS FILE MUST NOT BE EDITED UNDER ANY CIRCUNSTANCES.\n";
    print OUTFILE "# IT IS IGNORED BY libtrash AND ITS SINGLE PURPOSE IS TO PUBLICLY DISPLAY THE\n";
    print OUTFILE "# COMPILE-TIME DEFAULTS USED BY libtrash, SO THAT THE USERS CAN DECIDE WHICH\n";
    print OUTFILE "# SETTINGS THEY WISH TO OVERRIDE.\n";
    print OUTFILE "# TO OVERRIDE ONE OR MORE OF THESE SETTINGS, CREATE A PERSONAL CONFIGURATION\n";
    print OUTFILE "# FILE CALLED $personal_conf_file IN YOUR HOME DIRECTORY AND SPECIFY THE VALUES\n";
    print OUTFILE "# YOU WISH TO USE INSTEAD OF THE DEFAULTS SHOWN HERE. *YOU DON'T NEED TO REDEFINE\n";
    print OUTFILE "# ALL VARIABLES IN THAT FILE, ONLY THOSE WITH VALUES WHICH YOU WISH TO OVERRIDE.*\n";
    print OUTFILE "# EACH LINE IN THAT FILE MUST HAVE THE SAME FORMAT AS THE LINES IN THIS ONE,\n";
    print OUTFILE "# NAMELY:\n";
    print OUTFILE "#\n";
    print OUTFILE "# KEY = VALUE\n";
    print OUTFILE "#\n";
    print OUTFILE "# WHERE KEY IS THE NAME OF THE SETTING YOU WISH TO OVERRIDE, AND VALUE IS THE\n";
    print OUTFILE "# NEW, PREFERRED VALUE FOR THIS CONFIGURATION VARIABLE. DON'T USE QUOTES. TO\n";
    print OUTFILE "# DISABLE FEATURES SUCH AS TEMPORARY_DIRS, UNREMOVABLE_DIRS, REMOVABLE_MEDIA_\n";
    print OUTFILE "# MOUNT_POINTS AND IGNORE_EXTENSIONS, JUST FOLLOW THE EQUAL SIGN WITH A NEWLINE,\n";
    print OUTFILE "# E.G.:\n";
    print OUTFILE "#\n";
    print OUTFILE "# IGNORE_EXTENSIONS =\n";
    print OUTFILE "#\n";
    print OUTFILE "# THIS WOULD DISACTIVATE IGNORE_EXTENSIONS, I.E., IT WOULD PREVENT libtrash FROM\n";
    print OUTFILE "# DISCRIMINATING FILES BASED ON THEIR NAMES' EXTENSIONS. IF YOU NEED TO LIST\n";
    print OUTFILE "# DIFFERENT ITEMS (E.G., MORE THAN ONE DIRECTORY libtrash SHOULD IGNORE), USE A\n";
    print OUTFILE "# SEMI-COLON SEPARATED LIST - AGAIN, NO QUOTES ARE ALLOWED, NEITHER ARE SPACES\n";
    print OUTFILE "# BETWEEN THE LIST ITEMS / AROUND THE SEMI-COLONS:\n";
    print OUTFILE "#\n";
    print OUTFILE "# TEMPORARY_DIRS = /tmp;/var\n";
    print OUTFILE "#\n";
    print OUTFILE "# EMPTY LINES AND LINES STARTING WITH A '#' ARE IGNORED.\n\n\n";
    
}


open INFILE, "../libtrash.conf" or die "Unable to open file ../libtrash.conf.\n";
open OUTFILE, ">libtrash.conf.sys" or die "Unable to open file libtrash.conf.sys.\n";

my $conf_file_line = `grep PERSONAL_CONF_FILE ../libtrash.conf`;

$conf_file_line =~ /.+=\s*(\S+)/;

write_info($1);

my $ignore_lines = 1;

while (<INFILE>)
{
    print OUTFILE "$ARG" unless ($ignore_lines);
    
    $ignore_lines = 0 if (-1 != index($ARG, "[END OF COMPILE-TIME-ONLY SETTINGS]"));
    
}

close(INFILE);
close(OUTFILE);
