#!/usr/bin/perl -w
#
# run either
#  ./moviedrend.pl [-o output_prefix][--precolor soln.dsz]
# or
#  ./moviedrend.pl g/...[02468]00.dsz
#  ./moviedrend.pl g/...[05]00.dsz
# etc.
#
# BUGS: source solutions must be g/*.dsz
#
# use "--precolor" "xx_final.dsz" to preload the coloring like this:
# --loadSolution xx_final.dsz
# recolor
#
# Then, redirect the output to mov.drend and run:
# drend -px xx.mpout -B xx.gbin -d NN -f mov.drend

@g = ();
$none = 1;

$pre = "p/";
$precolor = undef;

while ( $arg = shift ) {
	if ( $arg eq "-o" ) {
		$pre = shift;
	} elsif ( $arg eq "--precolor" ) {
		$precolor = shift;
	} else {
		$none = 0;
		push @g, $arg;
	}
}
if ( $none ) {
	@g = <g/*.dsz>;
}
@gn = map {
	($a) = $_ =~ /g\/(.*)\.dsz/;
	$a;
} @g;

#print join(", ",@gn) . "\n";

if ( defined $precolor ) {
	print <<EOF;
--loadSolution $precolor
recolor
EOF
}

foreach $n ( @gn ) {
	print <<EOF;
--loadSolution g/${n}.dsz
--pngout ${pre}${n}.png
EOF
}
