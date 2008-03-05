#!/usr/bin/perl -w
$blockcount = 0;
$tpop = 0;
%th = ();
%cd106h = ();
while ( $line = <> ) {
	$block = substr( $line, 62, 4 );
	$pop = substr( $line, 292, 9 );
	$pop =~ s/ //g;
	if ( ($block) && ($block ne "    ") && ($pop) && ($pop > 0) ) {
		print $line;
		$tract = substr( $line, 55, 6 );
		$tract =~ s/ //g;
		$th{$tract}++;
		$cd106 = substr( $line, 136, 2 );
		$cd106h{$cd106} = 1;
		$blockcount++;
		$tpop += $pop;
	}
}
print STDERR "$blockcount blocks\n";
@tl = sort keys %th;
print STDERR "$#tl census tracts\n";
@d106s = sort keys %cd106h;
print STDERR "$#d106s (".join(",",@d106s).") congressional districts for 106\n";
print STDERR "total population: $tpop\n";
