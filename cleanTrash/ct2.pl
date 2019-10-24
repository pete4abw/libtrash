#! /usr/bin/perl
#
# File: ct2.pl
# Time-stamp: <02/08/14 19:47:36 martinc>
# $Id: ct2.pl,v 1.4 2002/08/14 18:47:51 martinc Exp $
#
# rewrite of cleanTrash (to rely less on external processes, and use
# cleaner perl)
#
# Copyright (C) 2002 University of Edinburgh
# Author: Martin Corley
#
# original code was:
# Copyright (C) 2001
#   Author: Daniel Sadilek <d.sadilek@globalview.de>,
#   Agency: Global View <http://www.globalview.de>, Berlin/Germany
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# See the file COPYING for details.

####################### USAGE ############################################
# one optional argument: -Q (operate quietly)
##########################################################################
use File::Find;

##########################################################################
### CONFIGURATION
##########################################################################
# Trash-directory relative to home-dir
$TRASH_DIR       = '/Desktop/Trash';

# Trash-history file relative to home-dir
$TRASH_HIST_FILE = '/.trashhist';

# maximum-Trash-size in kilo-bytes
$MAX_TRASH_SIZE  = 5000;

# files to ignore in Trash dir (KDE friendly!)
@IGNORE_TRASH    = ('.directory');

# minimum user number (by convention on many Linux systems, 'real' users
# have UIDs >= 500).  Set to zero to process all users
$MIN_USER        = 500;

##########################################################################
### PROGRAM
##########################################################################

$VERBOSE = 1;

if (@ARGV) {
  if (@ARGV == 1 && $ARGV[0] eq '-Q') {
    $VERBOSE=0;
  } else {
    print "'$ARGV[0]'\n";
    die "usage: $0 [-Q]";
  }
}

# parse password file looking for home directories
my %homes=get_dirs();

foreach my $home (keys %homes) {
  process_dir($home);
}


sub process_dir {
  my $home=shift;
  my $size=0;
  my %sizes;

  print "\nExamining $home$TRASH_DIR...\n"        if ($VERBOSE);
  unless (-e "$home$TRASH_DIR") {
    print "  -> no trash directory. Skipping.\n"  if ($VERBOSE);
    return;
  }
  my @histfiles=readhist($home);

  # find all regular files, depth first (so we can delete empty dirs)
  finddepth({wanted => \&process_file, no_chdir => 1},"$home$TRASH_DIR");
  # --------------------------------------
  # inline sub, so we can affect @newfiles
  {
    sub process_file {
      my $file = $_;
      my $ignore;
      ($ignore = $file) =~ s|^.*/||;
      if (-d $file) {
	rmdir($file); # this may fail but should eventually clean out dirs
      }
      return     unless (-f $file);
      return     if (in_list($ignore,@IGNORE_TRASH));
      unless (in_list($file,@histfiles)) {
	push (@histfiles,$file);
      }
    }
  }
  # --------------------------------------

  foreach my $file (@histfiles) {
    $sizes{$file}=getsize($file);
    $size+=$sizes{$file};
  }
  printf "  -> Trash has size %.2fk\n",$size            if ($VERBOSE);
  if ($size <= $MAX_TRASH_SIZE) {
    print "$home: OK\n";
  } else {
    print "$home: TOO BIG\n";
    $ENV{'TRASH_OFF'}='YES';
    open OLDERR,">&STDERR";
    open STDERR,">/dev/null";
    my $removed=0;
    foreach my $file (@histfiles) {
      last if ($size <= $MAX_TRASH_SIZE);
      if ( -e $file ) {
	unlink($file);
	$size-=$sizes{$file};
	$removed+=$sizes{$file};
	printf "  -> Deleting file %s (%.2fk)\n",$file,$sizes{$file}
	                                                if ($VERBOSE);
      }
    }
    open STDERR,">&OLDERR";
    printf "  -> %.2fk removed\n",$removed;
  }
  savehist($home,@histfiles);
}


sub get_dirs {
  my %homes;

  while (my ($name, $passwd, $uid, $gid,
	     $quota, $comment, $gcos, $dir, $shell) =  getpwent()) {
    $dir =~ s|/$||;    # remove trailing slash (if any)
    if ($uid >= $MIN_USER && $dir =~ m|/|) { # if in user space
                                             # and is a real directory
      $homes{$dir}++;
    }
  }
  return %homes;
}

sub readhist {
  my $home=shift;
  my @trashfiles=();

  if (open (HISTFILE,"<$home$TRASH_HIST_FILE")) {
    while (<HISTFILE>) {
      chomp;
      push @trashfiles,$_  if (-e $_);
    }
    close (HISTFILE);
  } else {
    print "  -> no history file in $home yet\n"    if ($VERBOSE);
  }
  return @trashfiles;
}

sub savehist {
  my ($home,@trashfiles) = @_;

  open (HISTFILE, ">$home$TRASH_HIST_FILE")
                             or die "Can't write to $home$TRASH_HIST_FILE";
  foreach my $file (@trashfiles) {
    if (-e $file) {
      print HISTFILE "$file\n";
    }
  }
  close(HISTFILE);
}

sub in_list {
  my ($item,@list) = @_;

  foreach my $entry (@list) {
    if ($item eq $entry) {
      return 1;
    }
  }
  return 0;
}

sub getsize {
  my $file=shift;

  my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
      $atime,$mtime,$ctime,$blksize,$blocks)
    = stat($file);
  return $size/1024;
}
