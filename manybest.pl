#!/usr/bin/perl -w

$usage = <<EOF;
usage: $0 [-n ngood][-bad badthresh][-d out-dir]
  [-rmbad][-rmempty][-mvbad]
  [statsum files, statlog files, or directories where they can be found]
  
If no statsum or statlog files are found through arguments,
./*/statsum are processed.
This is a reasonable default if mrun2.pl or runallstates.pl was used.

  -n ngood     Keep the top ngood solutions. Others may be partially or fully purged.
  -bad badthresh  Results with a Km/person score below badthresh may be purged.
  -d out-dir   Where to write out HTML and copy images to to show the best.
  -rmbad       Removes a large amount of intermediate data from bad results.
  -rmempty     Remove entirely solutions that are empty (likely solver bug)
  -mvbad       Move bad solutions (ngood or badthresh) into old/
EOF
$odir = "best";
@slogs = ();
$any = 0;
$nlim = undef;
$ngood = undef;
$badkmpp = undef;
@badlist = ();
$rmbad = 0;
$rmempty = 0;
$mvbad = 0;

while ( $arg = shift ) {
  if ( $arg eq "-d" ) {
    $odir = shift;
  } elsif ( $arg eq "-n" ) {
    $nlim = shift;
  } elsif ( $arg eq "-ngood" ) {
    $ngood = shift;
  } elsif ( $arg eq "-bad" ) {
    $badkmpp = shift;
  } elsif ( ($arg eq "-h") || ($arg eq "--help") ) {
    print $usage;
    exit 0;
  } elsif ( $arg eq "-rmbad" ) {
	$rmbad = 1;
  } elsif ( $arg eq "-rmempty" ) {
	$rmempty = 1;
  } elsif ( $arg eq "-mvbad" ) {
	$mvbad = 1;
  } elsif ( -f $arg ) {
    push @slogs, $arg;
    $any++;
  } elsif ( -f "$arg/statsum" ) {
    push @slogs, "$arg/statsum";
    $any++;
  } elsif ( -f "$arg/statlog" ) {
	  push @slogs, "$arg/statlog";
	  $any++;
  } else {
    print "bogus arg \"$arg\"\n";
    exit 1;
  }
}

if ( (defined $ngood) && (! defined $nlim) ) {
  $nlim = $ngood;
}
if ( ! $any ) {
  @slogs = <*/statsum>;
}
#print "slogs: " . join( ", ", @slogs ) . "\n";

if ( ! @slogs ) {
  print STDERR "no logs to process\n";
  print STDERR $usage;
  exit 1;
}

@they = ();

@empties = ();

SLOOP: foreach $fn ( @slogs ) {
  ($root) = $fn =~ /(.*)\/(?:statlog|statsum)/;
  if ( ! $root ) {
    print STDERR "could not find root of \"$fn\"\n";
    exit 1;
  }
  if ( $root eq "link1" ) {
    next SLOOP;
  }
  if ( ! -d $root ) {
    next SLOOP;
  }
  open FIN, $fn;
  my @lines = ();
  my $kmpp = undef;
  my $line;
  while ( $line = <FIN> ) {
    if ( ($tk) = $line =~ /Best Km\/p: ([0-9.]+)/ ) {
      $kmpp = $tk;
      ($lr) = $line =~ /^#(.*)/;
      push @lines, $lr;
    } elsif ( ($lr) = $line =~ /^#(.*)/ ) {
      push @lines, $lr;
    }
  }
  close FIN;
  if ( ! $kmpp ) {
#    print STDERR "no kmpp for $fn\n";
    push @empties, $root;
    next SLOOP;
  }
  $png = "${root}/bestKmpp.png";
  if ( ! -f $png ) {
    print STDERR "no $png\n";
    next SLOOP;
  }
  if ( (defined $badkmpp) && ($kmpp > $badkmpp) ) {
    push @badlist, $root;
  }
  push @they, [$root, $kmpp, $png, join("<br/>",@lines) ];
}

if ( ! @they ) {
  print STDERR "no good runs found\n";
  exit 1;
}

@they = sort { $a->[1] <=> $b->[1] } @they;

if ( ! -e $odir ) {
  mkdir( $odir ) or die "could not mkdir ${odir}: $!\n";
}

open( FOUT, '>', "${odir}/index.html" ) or die "could not open ${odir}/index.html: $!\n";

print FOUT<<EOF;
<html><head><title>$odir</title></head>
<body bgcolor="#ffffff">
<table border="1">
EOF

$i = 1;

OLP: foreach $t ( @they ) {
  print $t->[1] . "\t" . $t->[0] . "\n";
  system "cp $t->[2] $odir/$i.png";
  print FOUT<<EOF;
<tr><td><img src="$i.png"></td><td>run "$t->[0]"<br/>$t->[3]</td></tr>
EOF
  $i++;
  if ( (defined $nlim) && ($i > $nlim) ) {
    last OLP;
  }
}

print FOUT<<EOF;
</table>
</body></html>
EOF

close FOUT;

$t = $they[0];
if ( -e "link1" ) {
  if ( ! -l "link1" ) {
    print STDERR "link1 exists but is not a link\n";
  } else {
    unlink( "link1") or warn "rm link1 failed: $!";
    symlink( $t->[0], "link1" ) or warn "symlink($t->[0],link1) failed: $!";
  }
} else {
  symlink( $t->[0], "link1" ) or warn "symlink($t->[0],link1) failed: $!";
}

if ( (defined $ngood) && ($ngood <= $#they) ) {
  @badlist = map {$_->[0]} @they[$ngood .. $#they];
  print "badlist: " . join(" ", @badlist) . "\n";
}

@bgl = ();
foreach $b ( @badlist ) {
  if ( -e "${b}/g" ) {
    push @bgl, "${b}/g";
  }
  if ( -e "${b}/g.tar.bz2" ) {
    push @bgl, "${b}/g.tar.bz2";
  }
}
if ( @bgl ) {
  print "bad best kmpp:\n";
  @cmd = ("rm", "-rf", @bgl);
  if ( $rmbad ) {
    system @cmd;
  } else {
    print join(" ", @cmd) . "\n";
  }
}
if ( @empties ) {
  # don't delete the last one, in case it's still active
  pop @empties;
}
if ( @empties ) {
  print "empty solution:\n";
  @cmd = ("rm", "-rf", @empties);
  if ( $rmempty ) {
    system @cmd;
  } else {
    print join(" ", @cmd) . "\n";
  }
}
if ( @badlist ) {
  print "move bad best kmpp to old dir\n";
  @cmd = ("mv", @badlist, "old");
  if ( $mvbad ) {
    if ( ! -d "old" ) {
      mkdir("old") or die "could not mkdir(\"old\"): $!";
    }
    system @cmd;
  } else {
    print join(" ", @cmd) . "\n";
  }
}
