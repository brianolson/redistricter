#!/usr/bin/perl -w
# Create a HTML full of before-after pictures of states with current
# districting and solver's districting.
# Scans all two-letter directories for files resulting from manybest.pl
#
# See also baltx.pl to crate LaTeX output.
#
# Usage:
# ./bahtml.pl [--full][--data dir] > your.html
#   --full   emit complete HTML document, otherwise just a table to be
#            included in a larger document.
#   --data d where to find more data about states

$fullhtml = 0;
$table = 1;
$statsum = 1;
$datadir = undef;

while ( $arg = shift @ARGV ) {
  if ( $arg eq "--full" ) {
    $fullhtml = 1;
  } elsif ( $arg eq "--nostat" ) {
    $statsum = 0;
  } elsif ( $arg eq "--data" ) {
    $datadir = shift @ARGV;
  } else {
    print STDERR "bogus arg \"$arg\"\n";
    exit 1;
  }
}

if ( $fullhtml ) {
  print <<EOF;
<html><head><title>States Current and My Way</title>
<style>img{vertical-align: text-top;border: 0; padding: 0; margins: 0;}td.st{vertical-align: top; padding: 0; margins: 0;}h1,h2{text-align: center;}</style>
</head><body bgcolor="#ffffff" text="#000000">
<h1>State Congressional District Maps</h1>
<h2>Current and My Way</h2><center>
EOF
}
if ( $table ) {
  print "<table border=\"0\">";
}

$count = 0;
foreach $stu ( <??> ) {
	$statsum_part = "";
  if ( (-f "${stu}/link1/${stu}_ba_500.png") && 
       (-f "${stu}/link1/${stu}_ba.png") ) {
	$startstats = undef;
	if ((defined $datadir) and (-r "${datadir}/${stu}/${stu}_start_stats")) {
		if (open( FIN, '<', "${datadir}/${stu}/${stu}_start_stats")) {
			@lines = <FIN>;
			$ssdata = join("", @lines);
			($startkmpp) = $ssdata =~ /([0-9.]+) Km\/person/s;
			($max, $min) = $ssdata =~ /max=([0-9]+).*min=([0-9]+)/s;
			$startspread = $max - $min;
			($std) = $ssdata =~ /std=([0-9]+)/s;
			$startstats = "Km/p=${startkmpp} spread=${startspread} std=${std}";
		}
	}
	if ( $table ) {
		if ( $statsum ) {
			#@statsum_lines = ();
			if (open( FIN, '<', "${stu}/link1/statsum")) {
				local $dt = undef;
				local $bestkm = undef;
				while ( $line = <FIN> ) {
					$line =~ s/^#//;
					if ( ($t) = $line =~ /Best Km\/p: (.*)/ ) {
						$bestkm = $t;
						$bestkm =~ s/ gen=\d+//;
					} elsif ( ($t) = $line =~ /District calculation: ([0-9.]+ sec)/ ) {
						$dt = "run time: " . $t;
					}
					#push @statsum_lines, $line;
				}
				close FIN;
				#$statsum_part = "<td>" . join("<br />", @statsum_lines) . "</td>";
				if (defined $startstats) {
					$statsum_part = "<td>Current: $startstats<br />My Way: $bestkm<br />$dt</td>";
				} else {
					$statsum_part = "<td>$bestkm<br />$dt</td>";
				}
			} else {
				print STDERR "${stu}/link1/statsum: could not be opened, $!\n"; 
			}
		}
	print <<EOF;
<tr><td class=st>${stu}</td><td class=i><a href="${stu}/link1/${stu}_ba.png"><img src="${stu}/link1/${stu}_ba_500.png" alt="$stu current and proposed districting"></a></td>$statsum_part</tr>
EOF
    } else {
      print <<EOF;
${stu}: <a href="${stu}/link1/${stu}_ba.png"><img src="${stu}/link1/${stu}_ba_500.png" alt="$stu current and proposed districting"></a><br />
EOF
    }
    $count++;
  }
}

if ( $table ) {
  print "</table>";
}
if ( $fullhtml ) {
  print <<EOF;
</center>
<p align=center>$count states. AK, DE, MT, ND, SD, VT, WY have only 1 district.</p></body></html>
EOF
}
