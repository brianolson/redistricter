#!/usr/bin/perl -w

# create XX.make for some US State "XX"
# usage:
#  ./statemake.pl XX
#  ./statemake.pl CA
#  ./statemake.pl TX
# etc.

#$stu = "CA";

$tooldir = ".";

$stu = shift;
if ( ! $stu ) {
	print STDERR <<EOF;
usage: ./statemake.pl XX
	where "XX" is a two letter state postal code
EOF
	exit 1;
}

$stu =~ tr/a-z/A-Z/;
$stl = $stu;
$stl =~ tr/A-Z/a-z/;

sub mkdirMaybe($) {
	my $d = shift;
	if ( ! -e $d ) {
		mkdir $d;
	}
}

mkdirMaybe $stu;
mkdirMaybe "$stu/zips";
mkdirMaybe "$stu/raw";

open FOUT, '>', "${stu}/${stu}.make";

print FOUT <<EOF;
$stu.RT1:	${tooldir}/mergeRT1
	${tooldir}/mergeRT1 -o $stu.RT1 $stu/raw/*RT1

$stu.links $stu.edge:	${tooldir}/blockNeighbors.pl $stu.RT1
	${tooldir}/blockNeighbors.pl -i $stu.RT1 | ( tee $stu.info ) 2>&1

$stu.RTA:
	cat $stu/raw/*RTA > $stu.RTA

${stu}_108.uf1:	$stu.RTA ${tooldir}/cd108.pl ../data/${stl}101.uf1
	${tooldir}/cd108.pl -i $stu.RTA -u ../data/${stl}101.uf1

${stu}_all:	${stu}_108.uf1 $stu.links
EOF

if ( ! -e "$stu/zips/url" ) {
	open FU, '>', "$stu/zips/url";
	print FU "http://www2.census.gov/geo/tiger/tiger2004fe/$stu/\n";
	close FU;
}

close FOUT;
