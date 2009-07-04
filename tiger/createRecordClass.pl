#!/usr/bin/perl -w

use strict;

my $path = "record2";

while ( my $arg = shift ) {
	if ( $arg eq "-t" ) {
		$path = shift;
	} else {
		print STDERR "bogus arg \"$arg\"\n";
		exit 1;
	}
}

my @cp = split /\//, $path;
my $class = pop @cp;

open FIN, "${path}.txt";
open FH, '>', "${path}.h";
open FC, '>', "${path}.cpp";
open FP, '>', "${path}.py";

print FH <<EOF;
#ifndef ${class}_H
#define ${class}_H

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

class ${class} {
public:
	const uint8_t* base;
	${class}() : base( 0 ) {}
	${class}( const void* baseIn ) : base( (uint8_t*)baseIn ) {}
EOF
	
print FC <<EOF;
#include "${class}.h"

EOF

print FP <<EOF;
#!/usr/bin/python

class ${class}(object):
	def __init__(self, raw):
		self.raw = raw
	
EOF

sub print_field {
	my ($field,$bv,$fmt,$type,$beg,$end,$len,$desc) = @_;
	print <<EOF;
field: $field
bv: $bv
fmt: $fmt
type: $type
beg: $beg
end: $end
len: $len
desc: $desc
EOF
}
my @fields = ();
my $fieldwidth = 0;
while ( my $line = <FIN> ) {
# Field BV Fmt Type Beg End Len Description
	my ($field,$bv,$fmt,$type,$beg,$end,$len,$desc) = $line =~ /(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+([^\r\n]+\S)/;
#	print_field($field,$bv,$fmt,$type,$beg,$end,$len,$desc);
	if ( ! defined $desc ) {
		print STDERR "line failed to parse!\n-----------\n${line}-----------\n";
		print_field($field,$bv,$fmt,$type,$beg,$end,$len,$desc);
	} else {
	push @fields, [ $field,$bv,$fmt,$type,$beg,$end,$len,$desc ];
	if ( $end > $fieldwidth ) {
		$fieldwidth = $end;
	}
	}
}

# pad for "\r\n"
$fieldwidth += 2;

print FH <<EOF;
	static const int size;
	inline const char* record( int index ) const {
		return (const char*)(base + (index * $fieldwidth));
	}
EOF
print FC <<EOF;
const int ${class}::size = $fieldwidth;
EOF
print FP <<EOF;
	fieldwidth = $fieldwidth
	
	def record(self, x):
		start = ${class}.fieldwidth * x
		end = start + ${class}.fieldwidth
		return self.raw[start:end]
	
	def numRecords(self):
		return len(self.raw)/${class}.fieldwidth
	
EOF
foreach my $f ( @fields ) {
	my ($field,$bv,$fmt,$type,$beg,$end,$len,$desc) = @{$f};
	$beg--;
	$field =~ s/-/_/g;
print FH <<EOF;

	inline const char* $field( int index ) const {
		return (const char*)(base + (index * $fieldwidth) + $beg);
	}
	static const int ${field}_length;
	static const char* ${field}_desc;
EOF
print FP <<EOF;
	${field}_length = $len
	${field}_desc = '${desc}'
	
	def $field(self, x):
		start = (x * ${class}.fieldwidth) + $beg
		end = start + $len
		return self.raw[start:end]
	
EOF
	if ( $type eq "N" ) {
print FH <<EOF;
	inline long ${field}_longValue( int index ) const {
		char buf[$len + 1];
		memcpy( buf, base + (index * $fieldwidth) + $beg, $len );
		buf[$len] = 0;
		return strtol( buf, NULL, 10 );
	}
EOF
print FP <<EOF;
	def ${field}_int(self, x):
		return int(self.${field}(x).strip())
	
EOF
	} else {
		print FH <<EOF;
	inline char* ${field}_strdup( int index ) const {
		const char* it = (const char*)(base + (index * $fieldwidth) + $beg);
		int start, stop;
		start = 0;
		while ( it[start] == ' ' ) {
			start++;
			if ( start >= ${field}_length ) {
				// nothing there!
				return NULL;
			}
		}
		stop = ${field}_length - 1;
		while ( it[stop] == ' ' ) {
			stop--;
			if ( stop < start ) {
				// crazy!
				return NULL;
			}
		}
		int len = stop - start + 1;
		char* toret = (char*)malloc( len + 1 );
		memcpy( toret, it + start, len );
		toret[len] = 0;
		return toret;
	}
EOF
	}
#	inline int ${field}_length() const {
#		return $len;
#	}
print FC <<EOF;
const int ${class}::${field}_length = $len;
const char* ${class}::${field}_desc = "${desc}";
EOF
}

print FH <<EOF;
};

#endif
EOF

close FC;
close FH;
close FIN;
