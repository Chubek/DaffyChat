#!/usr/bin/env perl
use strict;
use warnings;
use File::Spec;
use Getopt::Long;

# OS-aware service path detection for DaffyChat
# Determines installation paths based on OS conventions

my %paths;
my $prefix = $ENV{DESTDIR} || '/usr/local';
my $config_format = 'json';
my $client_only = 0;
my $query;

GetOptions(
    'prefix=s'        => \$prefix,
    'config-format=s' => \$config_format,
    'client-only'     => \$client_only,
    'query=s'         => \$query,
) or die "Usage: $0 [--prefix PATH] [--config-format json|conf] [--client-only] [--query KEY]\n";

# Detect OS
my $os = $^O;
my $distro = '';

if ($os eq 'linux') {
    if (-f '/etc/os-release') {
        open my $fh, '<', '/etc/os-release' or die $!;
        while (<$fh>) {
            if (/^ID=(.+)/) {
                $distro = $1;
                $distro =~ s/"//g;
                last;
            }
        }
        close $fh;
    }
}

# Normalize prefix (expand ~)
if ($prefix =~ m{^~/(.*)}) {
    $prefix = File::Spec->catdir($ENV{HOME}, $1);
} elsif ($prefix eq '~') {
    $prefix = $ENV{HOME};
}

# Determine paths based on OS and prefix
sub is_system_prefix {
    my $p = shift;
    return $p =~ m{^/(usr(/local)?|opt)};
}

my $is_system = is_system_prefix($prefix);

# Binary directory
$paths{bindir} = File::Spec->catdir($prefix, 'bin');

# Library directory
$paths{libdir} = File::Spec->catdir($prefix, 'lib');

# Data directory
if ($is_system) {
    $paths{datadir} = File::Spec->catdir($prefix, 'share', 'daffychat');
} else {
    $paths{datadir} = File::Spec->catdir($prefix, 'share', 'daffychat');
}

# Config directory
if ($is_system) {
    if ($prefix eq '/usr/local') {
        $paths{configdir} = '/etc/daffychat';
    } elsif ($prefix eq '/usr') {
        $paths{configdir} = '/etc/daffychat';
    } elsif ($prefix =~ m{^/opt}) {
        $paths{configdir} = File::Spec->catdir($prefix, 'etc', 'daffychat');
    } else {
        $paths{configdir} = '/etc/daffychat';
    }
} else {
    $paths{configdir} = File::Spec->catdir($prefix, 'etc', 'daffychat');
}

# Systemd service directory
if ($is_system && $os eq 'linux') {
    if ($distro =~ /^(ubuntu|debian)/) {
        $paths{systemddir} = '/lib/systemd/system';
    } elsif ($distro =~ /^(fedora|rhel|centos|rocky|alma)/) {
        $paths{systemddir} = '/usr/lib/systemd/system';
    } elsif ($distro =~ /^arch/) {
        $paths{systemddir} = '/usr/lib/systemd/system';
    } else {
        $paths{systemddir} = '/etc/systemd/system';
    }
} elsif ($os eq 'linux') {
    $paths{systemddir} = File::Spec->catdir($ENV{HOME}, '.config', 'systemd', 'user');
} else {
    $paths{systemddir} = '';
}

# Documentation directory
if ($is_system) {
    $paths{docdir} = File::Spec->catdir($prefix, 'share', 'doc', 'daffychat');
} else {
    $paths{docdir} = File::Spec->catdir($prefix, 'share', 'doc', 'daffychat');
}

# Man page directory
if ($is_system) {
    $paths{mandir} = File::Spec->catdir($prefix, 'share', 'man');
} else {
    $paths{mandir} = File::Spec->catdir($prefix, 'share', 'man');
}

# Log directory
if ($is_system) {
    $paths{logdir} = '/var/log/daffychat';
} else {
    $paths{logdir} = File::Spec->catdir($ENV{HOME}, '.local', 'var', 'log', 'daffychat');
}

# Runtime directory
if ($is_system) {
    $paths{rundir} = '/var/run/daffychat';
} else {
    $paths{rundir} = File::Spec->catdir($ENV{HOME}, '.local', 'var', 'run', 'daffychat');
}

# State directory (for LMDB, etc.)
if ($is_system) {
    $paths{statedir} = '/var/lib/daffychat';
} else {
    $paths{statedir} = File::Spec->catdir($ENV{HOME}, '.local', 'var', 'lib', 'daffychat');
}

# Config file name
$paths{configfile} = $config_format eq 'conf' ? 'daffychat.conf' : 'daffychat.json';

# Client-only mode adjustments
if ($client_only) {
    # Client doesn't need systemd, logs, runtime, or state dirs
    $paths{systemddir} = '';
    $paths{logdir} = '';
    $paths{rundir} = '';
    $paths{statedir} = '';
}

# Query specific path
if ($query) {
    if (exists $paths{$query}) {
        print $paths{$query}, "\n";
    } else {
        die "Unknown path key: $query\n";
    }
    exit 0;
}

# Print all paths as shell variables
print "# DaffyChat installation paths for $os ($distro)\n";
print "# Prefix: $prefix\n";
print "# System install: ", ($is_system ? 'yes' : 'no'), "\n";
print "# Client-only: ", ($client_only ? 'yes' : 'no'), "\n";
print "\n";

for my $key (sort keys %paths) {
    my $value = $paths{$key};
    next unless $value;
    my $upper = uc($key);
    print "DAFFY_${upper}=\"$value\"\n";
}

print "\n# Export all\n";
print "export ", join(' ', map { "DAFFY_" . uc($_) } grep { $paths{$_} } sort keys %paths), "\n";
