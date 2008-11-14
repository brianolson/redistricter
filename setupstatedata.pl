#!/usr/bin/perl -w
#
$usage =<<EOF;
THIS SCRIPT MAY DOWNLOAD A LOT OF DATA

Some states may have 6 Gigabytes of data to download.
Don't all swamp the Census servers at once now.

Requires 'curl'.

Make a directory 'data' under the build directory.

Now run this script (from the installation directory, containing data/) with
a two letter postal abbreviation for a state:
./setupstatedata.pl ny
EOF
#
# BUGS: --dry-run mode will still create directories
#

$stl = undef;
$doit = 1;
$domake = 0;
$unpackall = 0;
$with_png = 1;
$protobuf = 1;

while ( $arg = shift ) {
	if ( $arg eq "--dry-run" || $arg eq "-n" ) {
		$doit = 0;
	} elsif ( $arg eq "--make" || $arg eq "-make" ) {
		$domake = 1;
	} elsif ( $arg eq "--nopng" || $arg eq "--without-png" ) {
		$with_png = 0;
	} elsif ( $arg eq "--unpackall" ) {
# an option useful if there were download problems
		$unpackall = 1;
	} elsif ( $arg eq "--gbin" ) {
		$protobuf = 0;
	} elsif ( ! defined $stl ) {
		$stl = $arg;
	} else {
		print STDERR "bogus arg \"$arg\" (or was \"$stl\" not the state arg?)\n";
		exit 1;
	}
}

sub nsystem(@){
  my @a = @_;
  my $cmd = join(" ",@a);
  system @a;
  my $st = $?;
  if ($st == -1) {
    print "failed to execute \`$cmd\`: $!\n";
    exit 1;
  } elsif ($st & 127) {
    printf "\`$cmd\` died with signal %d, %s coredump\n",
      ($st & 127),  ($st & 128) ? "with" : "without";
    exit 1;
  } elsif (($st >> 8) != 0) {
    printf "\`$cmd\` exited with value %d\n", $st >> 8;
    exit 1;
  }
}

if ( ! $stl ) {
  print STDERR $usage;
  exit 1;
}

$stl = lc($stl);
$stu = uc($stl);

#$usage =~ s/ny/$stl/g;
#$usage =~ s/NY/$stu/g;

if ( ! -d "data" ) {
  print STDERR<<EOF;
error, no directory "data", running this script from the install directory?

$usage
EOF
  exit 1;
}

$sf1index = "data/SF1index.html";
$sf1url = "http://ftp2.census.gov/census_2000/datasets/Summary_File_1/";
if ( ! -e $sf1index ) {
  nsystem "curl", "--silent", "-o", $sf1index, $sf1url;
}
$geourls = "data/geo_urls";
if ( ! -e $geourls ) {
  open FOUT, '>', $geourls;
  open(FIN, '<', $sf1index);
  while ($line = <FIN>) {
    if ( ($subdir) = $line =~ /href="([A-Z][^"]+)"/ ) {
      if ( $subdir ) {
#	print "${sf1url}${subdir}\n";
	open(FCU, "curl --silent ${sf1url}${subdir} |");
	while ( $cl = <FCU> ) {
	  if ( ($geo) = $cl =~ /href="(..geo_uf1.zip)"/ ) {
	    print FOUT "${sf1url}${subdir}${geo}\n";
	  }
	}
      }
    }
  }
  close FIN;
  close FOUT;
}

if ( ! -d "data/$stu" ) {
  mkdir("data/$stu") or die "could not mkdir \"data/$stu\": $!";
}

if ( ! (-f "data/${stu}/${stl}geo.uf1" || -f "data/${stu}/${stl}geo.uf1.bz2") ) {
  $zipfile = "data/${stu}/${stl}geo_uf1.zip";
  if ( ! -f $zipfile ) {
    open FIN, '<', $geourls;
    my @zipurl = grep(/${stl}geo_uf1.zip/, <FIN>);
    close FIN;
    if ($#zipurl == 0) {
      nsystem "curl", "--silent", "-o", $zipfile, $zipurl[0];
    } else {
      my $nz = $#zipurl + 1;
      print STDERR <<EOF;
no file data/${stu}/${stl}geo_uf1.zip and could not find url in $geourls
found $nz urls but wanted 1
EOF
      exit 1;
    }
  }
  if ( -f $zipfile ) {
    nsystem "(cd data/${stu}; unzip ${stl}geo_uf1.zip)";
    if ( ! -f "data/${stu}/${stl}geo.uf1" ) {
      print STDERR "have $zipfile but unzip did not produce \"data/${stu}/${stl}geo.uf1\"\n";
      exit 1;
    }
  } else {
    print STDERR<<EOF;
error, no file "data/${stu}/${stl}geo.uf1", have things been set up right?

$usage
EOF
    exit 1;
  }
}

$rootdir = `pwd`;
chomp $rootdir;

$tigerdir = "${rootdir}/tiger";

if ( 0 && ! -e "${tigerdir}/mergeRT1" ) {
	print "make in $tigerdir\n";
	if ( $doit ) {
		chdir( $tigerdir ) or die "could not chdir to ${tigerdir}: $!\n";
		nsystem "make";
		if ( ! -e "${tigerdir}/mergeRT1" ) {
			print STDERR<<EOF;
make failed to create missing executable 'mergeRT1', should be at
${tigerdir}/mergeRT1
EOF
			exit 1;
		}
		chdir( $rootdir ) or die "could not chdir to $rootdir : $!\n";
	}
}

if ( $domake || ! -e "linkfixup" ) {
#	print "make linkfixup\n";
	if ( $with_png ) {
		$makecmd = "make linkfixup tiger/makeLinks drend rta2dsz tiger/makepolys";
	} else {
		$makecmd = "make linkfixup tiger/makeLinks rta2dsz CXXFLAGS=-DWITH_PNG=0 LDPNG=";
	}
	print "$makecmd\n";
	if ( $doit ) {
		nsystem $makecmd;
		if ( ! -e "linkfixup" ) {
			print STDERR<<EOF;
make failed to create missing executable 'linkfixup', should be at
${rootdir}/linkfixup
EOF
			exit 1;
		}
	}
}

if ( ! -f "data/${stu}/url" ) {
  $tigerbase = "http://www2.census.gov/geo/tiger/";
  open FCIN, "curl --silent $tigerbase |";
  my @tref = ();
  my $tbl;
  my $href;
  my %edmap = ( 'f' => 1, 's' => 2, 't' => 3 );
  my $maxyear = undef;
  my $maxed = undef;
  while ( $tbl = <FCIN> ) {
    my ($year, $ed);
    if ( ($year, $ed) = $tbl =~ /href="tiger(\d\d\d\d)(.)e/ ) {
      print STDERR "tiger year=$year ed=$ed\n";
      if ( (! defined $maxyear) ||
	   ($year > $maxyear) ||
	   (($year == $maxyear) &&
	    ($edmap{$ed} > $edmap{$maxed})) ) {
	$maxyear = $year;
	$maxed = $ed;
      }
    }
  }
  close FCIN;
  if ( ! defined $maxyear ) {
    print STDERR <<EOF;
could not find tiger data url under $tigerbase
please explicitly set data/${stu}/url
EOF
  }
  $url = $tigerbase . "tiger" . $maxyear . $maxed . "e/" . $stu . "/";
  print STDERR "guessing tiger url:\n$url\nif this is wrong edit:\ndata/${stu}/url\n";
  open FURLOUT, '>', "data/${stu}/url";
  print FURLOUT "$url\n";
  close FURLOUT;
} else {
  open FU, "data/${stu}/url";
  #$url = "http://www2.census.gov/geo/tiger/tiger2005se/MA/";
  $url = <FU>;
  if ( $url ) {
    chomp $url;
  }
  close FU;
}

sub mkdirMaybe($) {
	my $d = shift;
	if ( ! -e $d ) {
		mkdir $d;
	}
}

mkdirMaybe "data/${stu}/zips";
mkdirMaybe "data/${stu}/raw";
chdir( "data/${stu}/zips" ) or die "could not chdir to data/${stu}/zips : $!\n";
if ( ! -f "index.html" ) {
#	print "fetch \"$url/index.html\"\n";
	print "fetch \"$url\"\n";
	if ( $doit ) {
		nsystem "curl", "-o", "index.html", $url;
	}
}

open FIN, "index.html";

@zl = ();
@zla = ();

while ( $line = <FIN> ) {
	if ( ($z) = $line =~ /href="(\S+.zip|counts\d*.txt)"/i ) {
		push @zla, $z;
		if ( ! -f $z ) {
			push @zl, $z;
		}
	}
}

close FIN;

if ( @zl ) {
	$zc = join ",", @zl;
	$zu = $url . "\{" . $zc . "\}";
	print "fetching $zu\n";
	if ( $doit ) {
		nsystem "curl", "-O", $zu;
	}
	print "unpacking data/${stu}/zips/*.zip to data/${stu}/raw\n";
	if ( $doit ) {
		foreach $z ( @zl ) {
			if ( $z =~ /.*zip$/i ) {
				nsystem "unzip", "$z", "*.RT[12AI]", "-d", "../raw";
			}
		}
	}
} elsif ( $unpackall ) {
	print "unpacking data/${stu}/zips/*.zip to data/${stu}/raw\n";
	if ( $doit ) {
		foreach $z ( @zla ) {
			if ( $z =~ /.*zip$/i ) {
				nsystem "unzip", "$z", "*.RT[12AI]", "-d", "../raw";
			}
		}
	}
} else {
	@rt1 = <../raw/*.RT1>;
	foreach $z ( @zla ) {
		if ( ($zr) = $z =~ /(.*)\.zip$/i ) {
			$irt1 = grep( /$zr/i, @rt1);
			if ( ! $irt1 ) {
				push @zl, $z;
			}
#nsystem "unzip", "$z", "*.RT[12AI]", "-d", "../raw";
		}
	}
	print "need to unpack: { " . join( ", ", @zl ) . " }\n";
	if ( $doit ) {
		foreach $z ( @zl ) {
			nsystem "unzip", "$z", "*.RT[12AI]", "-d", "../raw";
		}
	}
}

#chdir( $rootdir ) or die "could not chdir to $rootdir : $!\n";


# true if a newerthan b, or b doesn't exist
sub newerthan ($$) {
  my $mtimea;
  my $mtimeb;
  $mtimea = (stat("$_[0]"))[9];
  $mtimeb = (stat("$_[1]"))[9];
  if ( ! defined $mtimea ) {
    return 0;
  }
  if ( ! defined $mtimeb ) {
    return 1;
  }
  return $mtimea > $mtimeb;
};

chdir( "${rootdir}/data/${stu}" ) or die "could not chdir to ${rootdir}/data/${stu}: $!\n";

if ( newerthan( "${tigerdir}/makeLinks", "${stu}.links" ) ||
	 newerthan( "raw", "${stu}.links" ) ) {
	print "data/${stu}/raw/*RT1 -> ${stu}.links\n";
	if ( $doit ) {
		nsystem "${tigerdir}/makeLinks -o ${stu}.links raw/*RT1";
	}
}

if ( newerthan( "raw", "$stu.RTA" ) ) {
	print "merge raw/*RTA into $stu.RTA\n";
	if ( $doit ) {
		open FOUT, '>', "$stu.RTA";
		foreach $fi ( <raw/*RTA> ) {
			open FIN, $fi;
			while ( $line = <FIN> ) {
				print FOUT $line;
			}
			close FIN;
		}
		close FOUT;
	}
}

if ( (-f "${stl}geo.uf1.bz2") && (! -f "${stl}geo.uf1") ) {
	if ( newerthan( "${stl}geo.uf1.bz2", "${stl}101.uf1" ) ) {
		print "${stl}geo.uf1.bz2 -> ${stl}101.uf1\n";
		if ( $doit ) {
			open FIN, "bunzip2 --stdout ${stl}geo.uf1.bz2 |";
			open FOUT, '>', "${stl}101.uf1";
			while ( $line = <FIN> ) {
				if ( $line =~ /^uSF1  ..101/ ) {
					print FOUT $line;
				}
			}
			close FOUT;
			close FIN;
		}
	}
} else {
	if ( newerthan( "${stl}geo.uf1", "${stl}101.uf1" ) ) {
		print "${stl}geo.uf1 -> ${stl}101.uf1\n";
		if ( $doit ) {
			open FIN, '<', "${stl}geo.uf1";
			open FOUT, '>', "${stl}101.uf1";
			while ( $line = <FIN> ) {
				if ( $line =~ /^uSF1  ..101/ ) {
					 print FOUT $line;
				}
			}
			close FOUT;
			close FIN;
		}
	}
}

if ( $doit && ! -e ".uf1" ) {
	symlink "${stl}101.uf1", ".uf1";
}
if ( $doit && ! -e ".uf1.links" ) {
	symlink "${stu}.links", ".uf1.links";
}


if ($protobuf) {
	$stl_bin = $stl . ".pb";
} else {
	$stl_bin = $stl . ".gbin";
}

if ( newerthan( "${stl}101.uf1", $stl_bin ) ||
	 newerthan( "${stu}.links", $stl_bin ) ||
	 newerthan( "${rootdir}/linkfixup", $stl_bin ) ) {
	print "precompile { ${stl}101.uf1, ${stu}.links } to ${stl_bin}\n";
	if ($protobuf) {
		$linkfixupOutMode = "-p";
	} else {
		$linkfixupOutMode = "-o";
	}
	if ( $doit ) {
		nsystem "${rootdir}/linkfixup", "-U", ".uf1", $linkfixupOutMode, $stl_bin;
	}
}

if ( $doit && ! -e ".gbin" ) {
	symlink $stl_bin, ".gbin";
}

if ( newerthan( "raw", "measure" ) ||
     newerthan( "${rootdir}/measureRatio.pl", "measure") ) {
	print "measure state geometry\n";
	if ( $doit ) {
		nsystem "${rootdir}/measureRatio.pl raw/*RT1 raw/*RT2 raw/*RTA $stu > measure";
	}
}

if ( ! -e "handargs" ) {
  # only create this default if it doesn't exist.
  # may have hand tuned variables in it otherwise.
  open FOUT, '>', "handargs";
  print FOUT "-g 10000\n";
  close FOUT;
}

$distnumopt = "";

if ( $doit ) {
	open FIN, '<', "measure";
	while ( $line = <FIN> ) {
		if ( $line =~ /^-d/ ) {
			$distnumopt = $line;
			chomp $distnumopt;
		}
	}
	close FIN;
}

if ( newerthan( "${stu}.RTA", "${stl}109.dsz" ) ) {
	print "generate 109th Congress initial solution: ${stu}.RTA -> ${stl}109.dsz\n";
	if ( $doit ) {
		nsystem "${rootdir}/rta2dsz -B ${stl_bin} $distnumopt ${stu}.RTA -o ${stl}109.dsz";
	}
}
if ( $doit && $with_png ) {
	open FOUT, '>', ".make";
	print FOUT<<EOF;
-include data/${stu}/makedefaults
-include data/${stu}/makeoptions

data/${stu}/${stl_bin}:	data/${stu}/.uf1 linkfixup
	linkfixup -U data/${stu}/.uf1 -o data/${stu}/${stl_bin}

data/${stu}/${stl}.pbin:	data/${stu}/.uf1 linkfixup
	linkfixup -U data/${stu}/.uf1 -p data/${stu}/${stl}.pbin
	
${stl}_all:	data/${stu}/${stu}_start_sm.jpg data/${stu}/${stl}_sm.mpout data/${stu}/${stu}_start_sm.png

data/${stu}/${stu}_start_sm.jpg:	data/${stu}/${stu}_start.png
	convert data/${stu}/${stu}_start.png -resize 150x150 data/${stu}/${stu}_start_sm.jpg

data/${stu}/${stu}_start.png:	drend data/${stu}/${stl}.mpout data/${stu}/${stl}109.dsz
	./drend -B data/${stu}/${stl_bin} \$\{${stu}DISTOPT\} -px data/${stu}/${stl}.mpout --pngout data/${stu}/${stu}_start.png --loadSolution data/${stu}/${stl}109.dsz

data/${stu}/${stu}_start_sm.png:	drend data/${stu}/${stl}_sm.mpout data/${stu}/${stl}109.dsz
	./drend -B data/${stu}/${stl_bin} \$\{${stu}DISTOPT\} -px data/${stu}/${stl}_sm.mpout --pngout data/${stu}/${stu}_start_sm.png --loadSolution data/${stu}/${stl}109.dsz

data/${stu}/${stl}.mpout:	tiger/makepolys
	time tiger/makepolys -o data/${stu}/${stl}.mpout \$\{${stu}LONLAT\} \$\{${stu}PNGSIZE\} --maskout data/${stu}/${stl}mask.png data/${stu}/raw/*.RT1

data/${stu}/${stl}_sm.mpout:	tiger/makepolys
	time tiger/makepolys -o data/${stu}/${stl}_sm.mpout \$\{${stu}LONLAT\} \$\{${stu}PNGSIZE_SM\} --maskout data/${stu}/${stl}mask_sm.png data/${stu}/raw/*.RT1

data/${stu}/${stl}_large.mpout:	tiger/makepolys
	time tiger/makepolys -o data/${stu}/${stl}_large.mpout \$\{${stu}LONLAT\} \$\{${stu}PNGSIZE_LARGE\} --maskout data/${stu}/${stl}mask_large.png data/${stu}/raw/*.RT1

data/${stu}/${stl}_huge.mpout:	tiger/makepolys
	time tiger/makepolys -o data/${stu}/${stl}_huge.mpout \$\{${stu}LONLAT\} \$\{${stu}PNGSIZE_HUGE\} --maskout data/${stu}/${stl}mask_huge.png data/${stu}/raw/*.RT1

EOF
	close FOUT;
}

chdir( $rootdir ) or die "could not chdir \"$rootdir\": !$\n";
if ( $doit && $with_png ) {
	nsystem "make -k -f data/${stu}/.make ${stl}_all";
}
