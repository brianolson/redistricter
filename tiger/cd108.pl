#!/usr/bin/perl -w

%blocks = ();

$uf1 = undef;

#           1         2         3         4         5         6         7         8         9         0         1         2         3         4         5         6         7         8         9         0         
# 012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789
# A060425001c3430         2250010102004051A                             74385          1230004560                 1002667026                             12700   70900                      R                       
#                          sscccttttttbbbb                                                                        CC

$inname = "rt1.RTA";

while ( $arg = shift ) {
	if ( $arg eq "-i" ) {
		$inname = shift;
	} elsif ( $arg eq "-u" ) {
		$uf1 = shift;
	} elsif ( -r $arg ) {
		$inname = $arg;
	}
}

($inroot) = $inname =~ /(.*).RTA/;

if ( ! $inroot ) {
	print "inname $inname\ninroot $inroot\n";
	exit 1;
}

open FIN, '<', $inname;


$recordcount = 0;

$maxblock = 0;
$minblock = 9999999;
$maxtract = 0;
$mintract = 9999999;
$maxcounty = 0;
$mincounty = 99999;

sub statblock($$$) {
	my $goodblock = 1;
	my ( $cty, $tract, $block ) = @_;
	if ( $block !~ /^ +$/ ) {
		if ( $block > $maxblock ) {
			$maxblock = $block;
		}
		if ( $block < $minblock ) {
			$minblock = $block;
		}
	} else {
		$goodblock = 0;
	}
	if ( $tract !~ /^ +$/ ) {
		if ( $tract > $maxtract ) {
			$maxtract = $tract;
		}
		if ( $tract < $mintract ) {
			$mintract = $tract;
		}
	} else {
		$goodblock = 0;
	}
	if ( $cty !~ /^ +$/ ) {
		if ( $cty > $maxcounty ) {
			$maxcounty = $cty;
		}
		if ( $cty < $mincounty ) {
			$mincounty = $cty;
		}
	} else {
		$goodblock = 0;
	}
	return $goodblock;
}

while ( $line = <FIN> ) {
	$recordcount++;
	$rectype = substr( $line, 0, 1 );
	if ( $rectype ne "A" ) {
		die "not record type A\n";
	}
#	$state = substr( $line, 25, 2 );
	
	$county = substr( $line, 27, 3 );
	
	$tract = substr( $line, 30, 6 );
	$block = substr( $line, 36, 4 );
	
	$cong = substr( $line, 112, 2 );
	
	if ( statblock( $county, $tract, $block ) ) {
		my $l = $county . $tract . $block;
		my $oc = $blocks{$l};
		if ( $oc ) {
			if ( $oc ne $cong ) {
				print STDERR "$recordcount\tconflicting dup $l is dist $oc and $cong\n";
			} else {
#				print STDERR "$recordcount\tharmless dup $l is dist $oc and $cong\n";
			}
		}
		$blocks{$l} = $cong;
	}
}

close FIN;

@bl = keys %blocks;
$numblocks = $#bl + 1;

print <<EOF;
read $recordcount records
maxcounty $maxcounty
mincounty $mincounty
maxtract $maxtract
mintract $mintract
maxblock $maxblock
minblock $minblock
EOF

open FL, '>', "${inroot}.cd108";

print "cd108\n";
foreach $b ( sort @bl ) {
	$bk = $blocks{$b};
	print FL "$b$bk\n";
}

close FL;

if ( $uf1 && -r $uf1 ) {
	($uf1out) = $uf1 =~ /(.*)\.uf1$/;
	$uf1out .= "_108.uf1";
	print "$uf1 -> $uf1out\n";
	open FU, '<', $uf1;
	open FUO, '>', $uf1out;
	while ( $line = <FU> ) {
		$county = substr( $line, 31, 3 );
		$tract = substr( $line, 55, 6 );
		$block = substr( $line, 62, 4 );
		$v = $county . $tract . $block;
		$cd108 = $blocks{$v};
		if ( $cd108 ) {
		} else {
			$cd108 = substr( $line, 136, 2 );
			print STDERR "missing $v, using  cd106 '$cd108'\n";
		}
		substr( $line, 138, 2, $cd108 );
		print FUO $line;
	}
	close FU;
	close FUO;
}

print <<EOF;

blocks: $numblocks
EOF
