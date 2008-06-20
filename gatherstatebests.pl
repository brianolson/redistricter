#!/usr/bin/perl -w

$rooturl = "../../";

@states = ();

foreach $i ( <??/best/.part.html> ) {
  ($stu) = $i =~ /(..)\/best\/.part.html/;
  push @states, $stu;
}

@states = sort @states;

foreach $stu ( @states ) {
  open( FIN, '<', "${stu}/best/.part.html" ) or die "could not open ${stu}/best/.part.html: $!\n";
  @parts = <FIN>;
  close FIN;
  open( FOUT, '>', "${stu}/best/index.html" ) or die "could not open ${stu}/best/index.html: $!\n";
  print FOUT<<EOF;
<html><head><title>$stu</title></head>
<body bgcolor="#ffffff">
<p>
EOF
  $prevst = undef;
  $nextst = undef;
  @stlist = ();
  $prev = undef;
  $grabnext = 0;
  foreach $i ( @states ) {
    if ( $grabnext ) {
      $nextst = $i;
      $grabnext = 0;
    }
    if ( $i eq $stu ) {
      $prevst = $prev;
      $grabnext = 1;
      push @stlist, "<b>$i</b>";
    } else {
      push @stlist, "<a href=\"${rooturl}$i/best/index.html\">$i</a>";
    }
    $prev = $i;
  }
  if ( $prevst ) {
    print FOUT "<a href=\"${rooturl}$prevst/best/index.html\">prev</a> ";
  } else {
    print FOUT "<span style=\"color: white\">prev</span> ";
  }
  if ( $nextst ) {
    print FOUT "<a href=\"${rooturl}$nextst/best/index.html\">next</a>";
  } else {
    print FOUT "<span style=\"color: white\">next</span>";
  }
  print FOUT "</p><p>";
  print FOUT join(" ", @stlist);
  print FOUT "</p>";
  print FOUT join("",@parts);
  print FOUT<<EOF;
</body></html>
EOF
  close FOUT;
}
