#!/usr/bin/perl -w
# Expects to run in the build dir with data/?? containing state data.
# Using setupstatedata.pl in the standard way should do this.

$datadir = $ENV{DISTRICT_DATA};
if ( ! defined $datadir ) {
	$datadir = $ENV{PWD}."/data";
}
$bindir = undef;
$exe = undef;
$stdargs = " --blankDists --sLog g/ --statLog statlog ";
$start = time();
$end = undef;
@statearglist = ();
$extrargs = "";

while ( $arg = shift ) {
	if ( $arg eq "--data" ) {
		$datadir = shift;
		if ( ! -d $datadir ) {
			die "bogus data dir \"$datadir\"\n";
		}
	} elsif ( $arg eq "--bin" ) {
		$bindir = shift;
		if ( ! -x "${bindir}/districter2" ) {
			die "bogus bin dir \"$bindir\" does not contain districter2\n";
		}
	} elsif ( $arg eq "--exe" ) {
		$arg = shift;
		if ( ! -x $arg ) {
			die "bogus exe \"$arg\" is not executable\n";
		}
                $exe = $arg;
	} elsif ( $arg eq "--runsecs" ) {
		$arg = shift;
                $end = $start + $arg;
	} elsif ( $arg eq "--" ) {
		$extrargs = " " . shift;
	} else {
		local $astu = uc($arg);
		if ( -r "$datadir/$astu/basicargs" ) {
			push @statearglist, $astu;
		} else {
			print STDERR "$0: bogus arg \"$arg\"\n";
			exit 1;
		}
	}
}

if ( ! defined $exe ) {
  if ( ! defined $bindir ) {
    $bindir = $ENV{PWD};
  }
  $exe = $bindir . "/districter2";
  if ( ! -x $exe ) {
    die "bogus exe \"$exe\" is not executable\n";
  }
}

if ( @statearglist ) {
  @statedirs = map { "$datadir/$_" } @statearglist;
} else {
  @statedirs = <$datadir/??>;
}

@states = ();
%basicargs = ();
#%handargs = ();
%drendcmds = ();
SLOOP: foreach $s ( @statedirs ) {
  ($stu) = $s =~ /.*\/(..)/;
  if ( (! @statearglist) && (-e "$s/norun") ) {
    next SLOOP;
  }
  if ( open(FIN, '<', "$s/basicargs") ) {
    $ba = <FIN>;
    close FIN;
    chomp $ba;
    $ba =~ s/-B data/-B $datadir/;
  } else {
    next SLOOP;
  }
  if ( open(FIN, '<', "$s/drendcmd") ) {
    $dc = <FIN>;
    close FIN;
    chomp $dc;
    $dc =~ s/^#\s*//;
    $dc =~ s/\.\.\/data/$datadir/g;
    $dc =~ s/\.\.\/drend/${bindir}\/drend/;
    $dc =~ s/--pngout /--pngout link1\//;
    $dc =~ s/--loadSolution /--loadSolution link1\//;
    $drendcmds{$stu} = $dc;
  }
  push @states, $stu;
  $basicargs{$stu} = $ba;
#  $handargs{$stu} = $ha;
}

sub shuffle(@) {
  @out = ();
  while ( $#_ >= 0 ) {
    $i = int(rand($#_ + 1));
    push @out, $_[$i];
    $_[$i] = $_[$#_];
    pop @_;
  }
  return @out;
}
# run in a different order each time in case we do partial runs, spread the work
@states = shuffle(@states);

print join(" ",@states) . "\n";

sub timestamp() {
  my @t = localtime(time());
  my $year = $t[5] + 1900;
  my $mon = $t[4] + 1;
  return sprintf "%04d%02d%02d_%02d%02d%02d", $year, $mon, $t[3], $t[2], $t[1], $t[0];
}

if ( -x "/bin/nice" ) {
	$nice = "/bin/nice -n 20 ";
} elsif ( -x "/usr/bin/nice" ) {
	$nice = "/usr/bin/nice -n 20 ";
} else {
	$nice = "";
}
$rootdir = $ENV{PWD};

sub shouldstop() {
  if ( -e "${rootdir}/stop" ) {
    return 1;
  }
  if ( (defined $end) && (time() > $end) ) {
    return 1;
  }
  return 0;
}

while ( ! shouldstop() ) {
	sleep 1;
	ILP: foreach $stu ( @states ) {
	if ( ! -d "${stu}" ) {
		mkdir "${stu}" or die "could not mkidr \"${stu}\": !$\n";
	}
	$ha = "";
	# re-read handargs every time in case they change.
	$haname = "$datadir/$stu/handargs";
	if ( -r $haname ) {
		open(FIN, '<', $haname) or die "$haname: $!";
		$ha = <FIN>;
		close FIN;
		chomp $ha;
		$ha = " " . $ha;
	}
	$ctd = $stu . "/" . timestamp();
	mkdir( $ctd ) or die "could not mkdir $ctd: $!\n";
	mkdir( "$ctd/g" ) or die "could not mkdir $ctd/g: $!\n";
	chdir( $ctd ) or die "could not chdir to $ctd: $!\n";
	# Need to create a dummy regular file because of how kerberized perms work
	open( FOUTT, '>', "statsum" ) or die "could not create statsum: $!\n";
	close FOUTT;
	$cmd = $nice . $exe . $stdargs . $basicargs{$stu} . $ha . $extrargs;
	print $cmd . "\n";
	system $cmd;
	if ( ! -f "statlog" ) {
		die "run left no statlog. crazy.\n";
	}
	system "grep ^# statlog > statsum";
	system "gzip statlog";
# tar saves a lot of space vs the many small files collected and removed
	system "tar jcf g.tar.bz2 g";
	system "rm -rf g";
	if ( (defined $bindir) && (-x "$bindir/manybest.pl") && chdir("..") ) {
		system( "$bindir/manybest.pl", "-ngood", "15", "-mvbad", "-rmbad", "-rmempty", "-n", "10" );
		$drendcmd = $drendcmds{$stu};
		if ( (defined $drendcmd) && (! -f "link1/${stu}_final2.png") ) {
			print $drendcmd . "\n";
			system $drendcmd;
			$cmd = "convert $datadir/$stu/${stu}_start.png link1/${stu}_final2.png +append link1/${stu}_ba.png";
			print $cmd . "\n";
			system $cmd;
			$cmd = "convert link1/${stu}_ba.png -resize 500x500 link1/${stu}_ba_500.png";
			print $cmd . "\n";
			system $cmd;
		}
	}
# TODO: $datadir/$stu/drendcmd line 1, modified for datadir, bindir and link1
	chdir( $rootdir ) or die "could not chdir to $rootdir: $!\n";
	if ( shouldstop() ) {
		last ILP;
	}
	}
}
unlink "stop";
