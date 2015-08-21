#!/usr/bin/env perl
#
# vim:set ts=4 sw=4:

use strict;

my $DIR="l0dables";
my $memcpy;

if( -d "../$DIR"){
    chdir("..");
}

if ( ! -d $DIR ){
    die "Can't find $DIR?";
};

my @symb;
open(Q,"<","l0dables/EXPORTS") || die "$!";
while(<Q>){
    chomp;
	s/\r$//;
    next if /^#/;
    next if /^\s*$/;
    push @symb,$_;
};
close(Q);

open (C, ">", "$DIR/jumptable.c")    ||die;
open (H, ">", "$DIR/jumptable.h")    ||die;
open (D, ">", "$DIR/usetable.h")     ||die;

my %types;
my %files;
my %variable;

use File::Find ();

sub wanted { # Go through all headers
    my $id;
    next unless /\.h$/;
	open(F,"<",$_) || die;
	while(<F>){
		chomp;
		s/\r$//; 		 # Remove DOS line-endings
		s!//.*!!;		 # Remove comments(1)
		s!/\*[^/]*}*/!!; # Remove comments(2)
		if(m!^[^(]* ([\w]+)\s*\(.*\);\s*$!){ # Function
            $id=$1;
            s/$id/(*$id)/;
            s/;//;
			$types{$id}=$_;
			$files{$id}=$File::Find::name;
		}elsif (m!^\s*extern\s[^(]* ([\w]+)\s*(\[[^]]*\]\s*)?;\s*$!){ # (globla) Variable
            $id=$1;
            s/extern //;
            s/$id/(*$id)/;
            s/;//;
            $variable{$id}=1;
			$types{$id}=$_;
			$files{$id}=$File::Find::name;
		};
	};
	close(F);
}

File::Find::find({wanted => \&wanted}, qw(
		fatfs
		lpcapi
		r0ketlib
		rad1olib
		libopencm3/include/libopencm3/lpc43xx
		hackrf/firmware/common
		hackrf/firmware/hackrf_usb
	));

print H <<EOF;
/* This files is autogenerated by mktable.pl from l0dables/EXPORTS */

#include <stdint.h>

#ifndef __BASICCONFIG_H_
struct CDESC {
    void * _dummy;
};
#endif

#include <fatfs/ff.h>
#include <r0ketlib/fs_util.h>

typedef struct {
	uintptr_t identifier;
EOF

print C <<EOF;
/* This files is autogenerated by mktable.pl from l0dables/EXPORTS */

EOF

my %defs;
for (@symb){ # Add necessary includes
    if(!$defs{$files{$_}}){
		print C qq!#include "$files{$_}"\n!;
        $defs{$files{$_}}++;
	};
};

print C <<EOF;

#include "jumptable.h"
/* Needs to be marked "KEEP()" inside the linker script */
__attribute__ ((section(".jump"))) jtable JumpTable = {
	0xccc2015,
EOF

print D <<EOF;
/* This files is autogenerated by mktable.pl from l0dables/EXPORTS */

#include "jumptable.h"

/* hack to inform linke of jumptable size */
__attribute__ ((section(".jump"))) jtable JumpTableSize;

/* Absolute address in ram
   needs to be the same as JumpTable address in main firmware
   can be checked in the corresponging .map file */
const jtable * const JumpTable=(jtable*)0x10000114;

/* convenience defines */
EOF

print I <<EOF;
/* Autogenerated definitions for jumptable */
EOF

$\="\n";

for (@symb){
	if(!$types{$_}){
		warn "Couldn't find $_ - ignoring it.";
        print C "0xdeadc0de, ";
        print H "uintptr_t _dummy;";
        next;
	};

    if($variable{$_}){
        print C "	&$_,";
		print H "\t$types{$_};";
		print D "#define $_ (*JumpTable->$_)";
    }else{
        print C "	$_,";
		print H "\t$types{$_};";
		print D "#define $_ JumpTable->$_";
    };
};

print C "};";
print H "} jtable;";

close(D);
close(H);
close(C);

print "done.";
