#!/usr/bin/perl -w

$min = 999999;
$max = 0;
$sum = 0;
$count = 0;
$lines = 0;

%fileids = ();
%states = ();
%chariters = ();
%cifsn = ();

while ( $line = <> ) {
  $lines++;
  @a = split( /,/, $line );
  $fileids{$a[0]}++;
  $states{$a[1]}++;
  $chariters{$a[2]}++;
  $cifsn{$a[3]}++;
  foreach $x ( @a ) {
    if ( $x =~ /^\d+$/ ) {
      $count++;
      $sum += $x;
      if ( $x < $min ) {
        $min = $x;
      }
      if ( $x > $max ) {
        $max = $x;
      }
    }
  }
}

$avg = $sum / $count;
print <<EOF;
$lines lines, $count numeric elements
min: $min
avg: $avg
max: $max
EOF
print "fileids  : \'" . join("\', \'", keys %fileids) . "\'\n";
print "states   : \'" . join("\', \'", keys %states) . "\'\n";
print "chariters: \'" . join("\', \'", keys %chariters) . "\'\n";
print "cifsn    : \'" . join("\', \'", keys %cifsn) . "\'\n";
