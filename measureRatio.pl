#!/usr/bin/perl -w
# scan a .uf1 file and report the latitude and longitude bounding box stats

$minlat = 1000;
$maxlat = -1000;
$minlon = 1000;
$maxlon = -1000;

sub checkpt($$) {
	my $lon = shift;
	my $lat = shift;
	$lon = $lon / 1000000.0;
	$lat = $lat / 1000000.0;
	if ( $lon < $minlon ) {
		$minlon = $lon;
	}
	if ( $lon > $maxlon ) {
		$maxlon = $lon;
	}
	if ( $lat < $minlat ) {
		$minlat = $lat;
	}
	if ( $lat > $maxlat ) {
		$maxlat = $lat;
	}
}

%cds = ();

sub checkcd($) {
	my $tcd = shift;
	if ( $tcd =~ /^[ 0-9][0-9]$/ ) {
		$tcd =~ s/ /0/;
		$cds{$tcd} = 1;
	}
}

sub checkR2($) {
	my $fname = shift;
	open FIN, '<', $fname;
	my $line;
	while ( $line = <FIN> ) {
		for ( $i = 0; $i < 10; $i++ ) {
			$lon = substr( $line, 18 + ($i * 19), 10 );
			$lat = substr( $line, 28 + ($i * 19), 9 );
			if ( $lon eq "+000000000" && $lat eq "+00000000" ) {
# skip
			} else {
				checkpt( $lon, $lat );
			}
		}
	}
	close FIN;
}

sub checkR1($) {
	my $fname = shift;
	open FIN, '<', $fname;
	my $line;
	while ( $line = <FIN> ) {
		$lon = substr( $line, 190, 10 );
		$lat = substr( $line, 200, 9 );
		checkpt( $lon, $lat );
		$lon = substr( $line, 209, 10 );
		$lat = substr( $line, 219, 9 );
		checkpt( $lon, $lat );
	}
	close FIN;
}

sub checkRA($) {
	my $fname = shift;
	open FIN, '<', $fname;
	my $line;
	while ( $line = <FIN> ) {
		my $cd = substr( $line, 112, 2 );
		checkcd( $cd );
	}
	close FIN;
}

sub processUF1($) {
	my $fname = shift;
	open FIN, '<', $fname;
	my $line;
#my %cd106h = ();
	
	while ( $line = <FIN> ) {
		$lat = substr( $line, 310, 9 );
		$lon = substr( $line, 319, 10 );
		checkpt( $lon, $lat );
		
		$cd106 = substr( $line, 136, 2 );
		checkcd($cd106);
	}
	close FIN;
	
#	@d106s = sort keys %cd106h;
#	print STDERR "$#d106s (".join(",",@d106s).") congressional districts for 106\n";
}

$stu = undef;

while ( $arg = shift ) {
	if ( $arg =~ /RT1$/ ) {
		print STDERR "$arg\n";
		checkR1( $arg );
	} elsif ( $arg =~ /RT2$/ ) {
		print STDERR "$arg\n";
		checkR2( $arg );
	} elsif ( $arg =~ /uf1$/i ) {
		processUF1( $arg );
	} elsif ( $arg =~ /RTA$/ ) {
		print STDERR "$arg\n";
		checkRA( $arg );
	} elsif ( $arg =~ /^[A-Z][A-Z]$/ ) {
		$stu = $arg;
	} else {
		print STDERR "bogus arg \"$arg\"\n";
	}
}

if ( %cds ) {
	@cdl = sort keys %cds;
        $numdists = ($#cdl + 1);
	print "congressional districts: " . join(", ", @cdl) . "\n-d " . $numdists . "\n";
}

if ( ! defined $stu ) {
  print STDERR "need to define a state by 2 letter code";
  exit 1;
}
$stl = lc($stu);
$stu = uc($stu);

$dlat = $maxlat - $minlat;
$dlon = $maxlon - $minlon;
$rat = $dlon / $dlat;

print <<EOF;
    (minlon,maxlon), (minlat,maxlat)
bb: ($minlon,$maxlon), ($minlat,$maxlat)
--minlon $minlon --maxlon $maxlon --minlat $minlat --maxlat $maxlat

dlon/dlat
$dlon/$dlat
$rat
EOF

$avglat = ($maxlat + $minlat)/2;
$clat = cos( $avglat * 3.14159265358979323846 / 180.0 );
$dlon = $dlon * $clat;
$rat = $dlon / $dlat; # width / height

$w = 480 * $rat;
$h = 640 / $rat;

print <<EOF;
* cos(average latitue)\t* $clat
dlon/dlat
$dlon/$dlat
$rat
EOF
if ( $w > 640 ) {
  $basew = 640;
  $baseh = $h;
} else {
  $basew = $w;
  $baseh = 480;
}
printf("#--pngW %d --pngH %d\n", $w, 480 );
printf("#--pngW %d --pngH %d\n", 640, $h );
printf("#--pngW %d --pngH %d\n", $w * 4, 480 * 4 );
printf("#--pngW %d --pngH %d\n", 640 * 4, $h * 4 );
printf("--pngW %d --pngH %d\n", $basew * 4, $baseh * 4 );
printf("#--pngW %d --pngH %d\n", $w * 8, 480 * 8 );
printf("#--pngW %d --pngH %d\n", 640 * 8, $h * 8 );
printf("#--pngW %d --pngH %d\n", $w * 16, 480 * 16 );
printf("#--pngW %d --pngH %d\n", 640 * 16, $h * 16 );

$pngsize_sm = sprintf("--pngW %d --pngH %d", $basew, $baseh );
$pngsize = sprintf("--pngW %d --pngH %d", $basew * 4, $baseh * 4 );
$pngsize_large = sprintf("--pngW %d --pngH %d", $basew * 8, $baseh * 8 );
$pngsize_huge = sprintf("--pngW %d --pngH %d", $basew * 16, $baseh * 16 );

$distnumopt = "-d " . ($#cdl + 1);
open FOUT, '>', 'makedefaults';
print FOUT<<EOF;
${stu}PNGSIZE ?= $pngsize
${stu}PNGSIZE_SM ?= $pngsize_sm
${stu}PNGSIZE_LARGE ?= $pngsize_large
${stu}PNGSIZE_HUGE ?= $pngsize_huge
${stu}LONLAT ?= --minlon $minlon --maxlon $maxlon --minlat $minlat --maxlat $maxlat
${stu}DISTOPT ?= $distnumopt
EOF
close FOUT;

open FOUT, '>', "drendcmd";
print FOUT<<EOF;
# ../drend -B ../data/${stu}/.gbin $distnumopt $pngsize --loadSolution bestKmpp.dsz -px ../data/${stu}/${stl}.mpout --pngout ${stu}_final2.png
# ../drend -B ../data/${stu}/.gbin $distnumopt -f ../data/${stu}/drendcmd
-px ../data/${stu}/${stl}.mpout
--loadSolution bestKmpp.dsz
recolor
$pngsize
--pngout ${stu}_final2.png
--loadSolution ../data/${stu}/${stl}109.dsz
--pngout ${stu}_start.png
EOF
close FOUT;

open FOUT, '>', "mrun${stl}";
print FOUT <<EOF;
#!/bin/sh -x
exec ./mrun2.pl -d $numdists $pngsize_sm ${stu}
EOF
close FOUT;

open FOUT, '>', "basicargs";
print FOUT <<EOF;
-B data/${stu}/${stl}.gbin --pngout ${stu}_final.png -d $numdists -o ${stu}_final.dsz $pngsize_sm
EOF
close FOUT;
# mrun2.pl also adds: --blankDists --sLog g/ --statLog statlog
# need to also specify -g nnnnn --popRatioFactorPoints a,b,c,d
# might have to s/-B data/-B ${somepath}data/ on the above string
