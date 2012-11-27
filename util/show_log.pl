#!/usr/bin/perl -n

BEGIN {
	my $hex = qr/[a-fA-F0-9]/;

	sub toLine($) {
		my $h = shift;

		if    ($h =~ /^8/) { $exe = 'build/utente';  }
		elsif ($h =~ /^4/) { $exe = 'build/io';      }
		else               { $exe = "build/sistema"; }

		my $out = `addr2line -Cfe $exe $h`;
		if ($?) {
			return $h;
		}
		chomp $out;
		my @lines = split(/\n/, $out);
		if ($lines[1] =~ /^\?\?/) {
			return $h;
		}
		my $s = '';
		$lines[1] =~ s#^.*/##;
		if ($lines[0] ne '??') {
			$s .= $lines[0];
		}
		$s .= ' [' . $lines[1] . ']';
		return $s;
	}

	sub decodeAddr($) {
		my $s = shift;
		$s =~ s#(?<!$hex)($hex{8})(?!$hex)#&toLine($1)#meg;
		return $s;
	}
}

chomp;
my ($level, $id, $msg) = split /\t/;
if ($level ne "USR") { 
	my $s = &decodeAddr($msg);
	print "$level\t$id\t$s\n";
}
