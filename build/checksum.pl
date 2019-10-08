#!/usr/bin/perl -w
# Accept a comma-separated string as command-line argument
# In this string, the comma is interpreted as a TAB
#
use strict;
use warnings;

my $cks = 0;
if( scalar @ARGV != 1 ){
	print "Usage: ".$0." <information_group>\n";
} else {
	my $igroup = $ARGV[0];
	my $len = length( $igroup ) - 1;
	for( my $i=0 ; $i<$len ; ++$i ){
		my $c = substr( $igroup, $i, 1 );
		if( $c eq ',' ){
			$cks += 9;
		} else {
			$cks += ord( $c );
		}
	}
	$cks &= 0x3f;
	$cks += 0x20;

	#print "group='".$igroup."', chksum=0x".hex( ord( substr( $igroup, -1 ))).", computed=0x".hex( $cks )."\n";
	print "group='".$igroup."', chksum=".ord( substr( $igroup, -1 )).", computed=".$cks."\n";
}
