#!/usr/bin/perl -w
use Compress::Zlib;
$gz = undef;
if ( -f "statlog" ) {
	open( FIN, "statlog" ) or die "cannot open statlog: $!\n";
} elsif ( -f "statlog.gz" ) {
	$gz = gzopen("statlog.gz","rb") or die "cannot open statlog.gz: !$\n";
} else {
	die "neither statlog nor statlog.gz found";
}

sub statlogline() {
	my $line;
	if ( defined $gz ) {
		my $bytesread = $gz->gzreadline($line);
		if ( $bytesread < 0 ) {
			return undef;
		}
		return $line;
	}
	return <FIN>;
}
@kmpp = ();
@std = ();
@spread = ();
@nodist = ();
$gen = -1;
while ( $line = statlogline() ) {
#generation: 0
#generation 0: 63.4341035 Km/person
#population avg=651619 std=0.491869377
#max=651620 (district 3)  min=651619 (district 1)  median=651619 (district 17)
  if ( ($tkp) = $line =~ /gen(?:eration|) (\d+)/ ) {
    $gen = $tkp;
  }
  if ( ($tkp) = $line =~ /([0-9.]+) Km\/person/ ) {
    push @kmpp, $tkp;
  }
  if ( ($tkp) = $line =~ /std=([0-9.]+)/ ) {
    push @std, $tkp;
  }
  if ( ($tmax,$tmin) = $line =~ /max=([0-9.]+).*min=([0-9.]+)/ ) {
    $tsp = $tmax - $tmin;
    push @spread, $tsp;
  }
  if ( ($tkp) = $line =~ /in no district \(pop=([0-9.]+)\)/ ) {
    push @nodist, $tkp;
  }
}
close FIN;

$nkmpp = $#kmpp;
$nstd = $#std;
$nspread = $#spread;
if ( ($nkmpp != $nstd) || ($nstd != $nspread) ) {
  print STDERR <<EOF;
error: have $nkmpp Km/person results
            $nstd std results
            $nspread spread results
EOF
 exit 1;
}
open GP, "|gnuplot";
print GP <<EOF;
set logscale y
set style data lines
set terminal png
set output 'kmpp.png'
plot '-' title 'Km/person'
EOF
#set size 2,2
foreach $x ( @kmpp ) {
  print GP "$x\n";
}
print GP <<EOF;
e
set output 'statlog.png'
plot '-' title 'std','-' title 'spread'
EOF
foreach $x ( @std ) {
  print GP "$x\n";
}
print GP "e\n";
foreach $x ( @spread ) {
  print GP "$x\n";
}
print GP "e\n";
if ( $#nodist > 2 ) {
print GP <<EOF;
set output 'nodist.png'
set ylabel 'people'
plot '-' title 'no dist'
EOF
foreach $x ( @nodist ) {
  print GP "$x\n";
}
}
close GP;
