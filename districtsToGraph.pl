#!/usr/bin/perl -w

$zipfile = "CA_zcta5";
@zipd = ();
open FOUT, ">dd.graph";
print FOUT "!defaultDrawLink 1\n";

sub stripwhitespace($) {
	my $s = shift;
	my $r;
	($r) = $s =~ /^\s*(\S.*\S)\s*$/;
	if ( ! defined $r ) {
		return "";
	}
	return $r;
}

sub namefix($) {
	my $s = shift;
	$s =~ s/ /_/g;
	$s =~ s/-/_/g;
	return $s;
}

open ZIN, "$zipfile";
while ( $line = <ZIN> ) {
	@parts = $line =~ 
/(..)(................................................................)(.........)(.........)(..............)(..............)(............)(............)(..........)(...........)/;
# 12  3456789012345678901234567890123456789012345678901234567890123456  789012345  678901234  56789012345678  90123456789012  345678901234  567890123456  7890123456  78901234567
#            1         2         3		   4         5         6           7           8           9           0		 1           2           3           4           5
	($state,$name,$pop,$houseu,$lareamm,$wareamm,$lareamimi,$wareamimi,$lat,$lon) = map( stripwhitespace($_), @parts );
#	@parts = map( stripwhitespace($_), @parts );
#	print "<tr><td>" . join( "</td><td>", @parts ) . "</td></tr>\n";
	$name = namefix( $name );
	push @zipd, [$state,$name,$pop,$houseu,$lareamm,$wareamm,$lareamimi,$wareamimi,$lat,$lon];
	if ( $name ) {
		if ( $lon && $lat ) {
			print FOUT "!nodePos $lon $lat 0\n";
		}
		print FOUT "$name\n";
	}
}
close ZIN;

open FID, "dd";
while ( $line = <FID> ) {
	($dn, $zlt, $pop) = $line =~ /district (\d+): (\d[0-9 ]+\d) \(pop=([-0-9.]+)\)/;
	print "New District $dn (population $pop):\n";
	@zl = split ' ', $zlt;
	for ( $i = 0; $i <= $#zl; $i++ ) {
		$zliname = ${$zipd[$zl[$i]]}[1];
		print FOUT "$zliname\n";
		print "\t$zliname\n";
		for ( $j = $i + 1; $j <= $#zl; $j++ ) {
			print FOUT "\t${$zipd[$zl[$j]]}[1]\n";
		}
	}
#	print "$dn $pop\n";
}
print "\n";
close FID;
close FOUT;

