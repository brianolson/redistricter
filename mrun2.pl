#!/usr/bin/perl -w
# Expects the state data to be in ./data/?? unless otherwise specified.
# Wherever it is, the state data directory should be the result of
# setupstatedata.pl

$st="CA";
$dists = 53;
#$pngW = 415;
#$pngH = 480;
#$g = 250001;
$datadir = $ENV{DISTRICT_DATA};
if ( ! defined $datadir ) {
	$datadir = $ENV{PWD}."/data";
}
$bindir = $ENV{PWD};
$datafile = undef;
@extra_args = ();
$extramode = 0;

%pass_through = ();
foreach $i ("--pngW", "--pngH", "-g", "--popRatioFactorPoints") { $pass_through{$i} = 1; }

while ( $arg = shift ) {
	if ( $pass_through{$arg} ) {
		push @extra_args, $arg;
		$arg = shift;
		push @extra_args, $arg;
	} elsif ( $arg eq "-d" ) {
		$dists = shift;
	} elsif ( $arg eq "--data" ) {
		$datadir = shift;
		if ( ! -d $datadir ) {
			die "bogus data dir \"$datadir\"\n";
		}
	} elsif ( $arg eq "--datafile" ) {
		$datafile = shift;
		if ( ! -r $datafile ) {
			die "bogus data file \"$datafile\"\n";
		}
	} elsif ( $arg eq "--bin" ) {
		$bindir = shift;
		if ( ! -x "${bindir}/districter2" ) {
			die "bogus bin dir \"$bindir\" does not contain districter2\n";
		}
	} elsif ( $arg =~ /[a-zA-Z][a-zA-Z]/ ) {
		$st = $arg;
	} elsif ( $arg eq "--" ) {
		$extramode  = 1;
	} elsif ( $extramode ) {
		push @extra_args, $arg;
	} else {
		print STDERR "$0: bogus arg \"$arg\"\n";
		exit 1;
	}
}

if ( ! defined $datafile ) {
  $datafile = "${datadir}/${st}/.gbin";
}
if ( ! -r $datafile ) {
	print STDERR "no data for state \"$arg\" at $datafile\n";
	exit 1;
}

sub timestamp() {
  my @t = localtime(time());
  my $year = $t[5] + 1900;
  my $mon = $t[4] + 1;
  return sprintf "%04d%02d%02d_%02d%02d%02d", $year, $mon, $t[3], $t[2], $t[1], $t[0];
}

if ( -x "/bin/nice" ) {
	@nice = ("/bin/nice", "-n", "20");
} elsif ( -x "/usr/bin/nice" ) {
	@nice = ("/usr/bin/nice", "-n", "20");
} else {
	@nice = ();
}
$rootdir = $ENV{PWD};

while ( ! -e "stop" ) {
	sleep 1;
	if ( ! -d "${st}$dists" ) {
		mkdir "${st}$dists" or die "could not mkidr \"${st}$dists\": !$\n";
	}
	$ctd = $st . $dists . "/" . timestamp();
	mkdir( $ctd ) or die "could not mkdir $ctd: $!\n";
	mkdir( "$ctd/g" ) or die "could not mkdir $ctd/g: $!\n";
	chdir( $ctd ) or die "could not chdir to $ctd: $!\n";
	@cmd = ( @nice, "${bindir}/districter2", "-B", $datafile, "--pngout", "${st}_final.png", "--blankDists", "-d", $dists, "-o", "${st}_final.dsz", "--sLog", "g/", "--statLog", "statlog", @extra_args );
	print "@cmd\n";
	system @cmd;
	system "grep ^# statlog > statsum";
	system "gzip statlog";
# tar saves a lot of space vs the many small files collected and removed
	system "tar jcf g.tar.bz2 g";
	system "rm -rf g";
	chdir( $rootdir ) or die "could not chdir to $rootdir: $!\n";
}
unlink "stop";
