#!/usr/bin/perl -w

$min = 999999;
$max = 0;
$sum = 0;
$count = 0;
$lines = 0;

while ( $line = <> ) {
  $lines++;
  foreach $x ( split( /,/, $line ) ) {
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
