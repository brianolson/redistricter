#!/usr/bin/perl -w

%links = ();
%boundaryBlocks = ();
%blocks = ();

$inname = "rt1.RT1";

while ( $arg = shift ) {
	if ( $arg eq "-i" ) {
		$inname = shift;
	} elsif ( -r $arg ) {
		$inname = $arg;
	}
}

($inroot) = $inname =~ /(.*).RT1/;

if ( ! $inroot ) {
	print "inname $inname\ninroot $inroot\n";
	exit 1;
}

open FIN, '<', $inname;

$prevlineid = undef;

$recordcount = 0;

$maxblock = 0;
$minblock = 9999999;
$maxtract = 0;
$mintract = 9999999;
$maxcounty = 0;
$mincounty = 99999;
$rl = 0;
$lr = 0;

sub statblock($$$) {
	my ( $cty, $tract, $block ) = @_;
	if ( $block !~ /^ +$/ ) {
		if ( $block > $maxblock ) {
			$maxblock = $block;
		}
		if ( $block < $minblock ) {
			$minblock = $block;
		}
		$bs = $cty . $tract . $block;
		$blocks{$bs} = 1
	}
	if ( $tract !~ /^ +$/ ) {
		if ( $tract > $maxtract ) {
			$maxtract = $tract;
		}
		if ( $tract < $mintract ) {
			$mintract = $tract;
		}
	}
	if ( $cty !~ /^ +$/ ) {
		if ( $cty > $maxcounty ) {
			$maxcounty = $cty;
		}
		if ( $cty < $mincounty ) {
			$mincounty = $cty;
		}
	}
}

while ( $line = <FIN> ) {
	$recordcount++;
	$rectype = substr( $line, 0, 1 );
	if ( $rectype ne "1" ) {
		die "not record type 1\n";
	}
	$statel = substr( $line, 130, 2 );
	$stater = substr( $line, 132, 2 );

	$countyl = substr( $line, 134, 3 );
	$countyr = substr( $line, 137, 3 );

	$tractl = substr( $line, 170, 6 );
	$tractr = substr( $line, 176, 6 );
	$blockl = substr( $line, 182, 4 );
	$blockr = substr( $line, 186, 4 );

	$lineid = substr( $line, 5, 10 );

	if ( defined $prevlineid ) {
		if ( $prevlineid gt $lineid ) {
			print "prev gt!\n";
		}
	}
	statblock( $countyr, $tractr, $blockr );
	statblock( $countyl, $tractl, $blockl );
	if ( $statel ne $stater ) {
		if ( $blockl =~ /^\s+$/ ) {
			$l = $countyr . $tractr . $blockr . $lineid;
		} else {
			$l = $countyl . $tractl . $blockl . $lineid;
		}
		$boundaryBlocks{$l} = 1;
	} elsif ( $countyr ne $countyl || $tractr ne $tractl || $blockr ne $blockl ) {
# always put the lower value on the left, to prevent double entries
		if ( $countyr lt $countyl || $tractr lt $tractl || $blockr lt $blockl ) {
			$rl++;
			$l = $countyr . $tractr . $blockr . $countyl . $tractl . $blockl; # . $lineid;
		} else {
			$lr++;
			$l = $countyl . $tractl . $blockl . $countyr . $tractr . $blockr; # . $lineid;
		}
		$links{$l} = 1;
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
rl $rl
lr $lr
EOF

open FL, '>', "${inroot}.links";

print "links\n";
$linkCount = 0;
while ( ($l) = each( %links ) ) {
#	print "$l\n";
	if ( 0 ) {
	$countyl = substr( $l, 0, 3 );
	$countyr = substr( $l, 13, 3 );
	if ( $countyl ne $countyr ) {
		print FL "$l\n";
	} else {
#		print FL "$l\n";
	}
	} else {
		print FL "$l\n";
	}
	$linkCount++;
}

close FL;

open FE, '>', "${inroot}.edge";

print "\nedge nodes\n";
$edgeCount = 0;
while ( ($l) = each( %boundaryBlocks ) ) {
	print FE "$l\n";
	$edgeCount++;
}
close FE;

print <<EOF;

blocks: $numblocks
links: $linkCount
edges: $edgeCount
EOF
