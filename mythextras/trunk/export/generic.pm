#!/usr/bin/perl -w
#Last Updated: 2005.02.13 (xris)
#
#  generic.pm
#
#    generic routines for exporters
#

package export::generic;

    use Time::HiRes qw(usleep);
    use POSIX;

    use nuv_export::shared_utils;
    use nuv_export::ui;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &fork_command &has_data &fifos_wait
                        /;
    }

# Load the following extra parameters from the commandline
    add_arg('path:s',                        'Save path (only used with the noserver option)');
    add_arg('cutlist|use_cutlist!',          'Use the myth cutlist (or not)');

# These aren't used by all modules, but the routine to define them is here, so here they live
    add_arg('height|v_res|h=i',              'Output height.');
    add_arg('width|h_res|w=i',               'Output width.');
    add_arg('deinterlace:s!',                'Deinterlace video.');
    add_arg('noise_reduction|denoise|nr:s!', 'Enable noise reduction.');
    add_arg('crop!',                         'Crop out broadcast overscan.');

# Gather generic export settings
    sub gather_settings {
        my $self = shift;
    # Load the save path, if requested
        $self->{'path'} = query_savepath();
    # Ask the user if he/she wants to use the cutlist
        $self->{'use_cutlist'} = query_text('Enable Myth cutlist?',
                                            'yesno',
                                            (defined($Args{'cutlist'}) && $Args{'cutlist'} == 0) ? 'No' : 'Yes');
    # Video settings
        if (!$self->{'audioonly'}) {
        # Defaults?
            $Args{'denoise'}     = '' if (defined $Args{'denoise'} && $Args{'denoise'} eq '');
            $Args{'deinterlace'} = 1  if (defined $Args{'deinterlace'} && $Args{'deinterlace'} eq '');
        # Noise reduction?
            $self->{'denoise'} = query_text('Enable noise reduction (slower, but better results)?',
                                                    'yesno',
                                                    $self->{'denoise'} ? 'Yes' : 'No');
        # Deinterlace video?
            $self->{'deinterlace'} = query_text('Enable deinterlacing?',
                                                'yesno',
                                                $self->{'deinterlace'} ? 'Yes' : 'No');
        # Crop video to get rid of broadcast padding
            if ($Args{'crop'}) {
                $self->{'crop'} = 1;
            }
            else {
                $self->{'crop'} = query_text('Crop broadcast overscan (2% border)?',
                                             'yesno',
                                             $self->{'crop'} ? 'Yes' : 'No');
            }
        }
    }

# Check for a duplicate filename, and return a full path to the output filename
    sub get_outfile {
        my $self    = shift;
        my $episode = shift;
        my $suffix  = ($self->{'suffix'} or shift);
    # Build a clean filename
        my $outfile;
        if ($episode->{'outfile'}{$suffix}) {
            $outfile = $episode->{'outfile'}{$suffix};
        }
        else {
            if ($episode->{'show_name'} ne 'Untitled' and $episode ne 'Untitled') {
                $outfile = $episode->{'show_name'}.' - '.$episode->{'title'};
            }
            elsif($episode->{'show_name'} ne 'Untitled') {
                $outfile = $episode->{'show_name'};
            }
            elsif($episode ne 'Untitled') {
                $outfile = $episode->{'title'};
            }
            else {
                $outfile = 'Untitled';
            }
            $outfile =~ s/(?:[\/\\\:\*\?\<\>\|\-]+\s*)+(?=[^\s\/\\\:\*\?\<\>\|\-])/- /sg;
            $outfile =~ tr/"/'/s;
        # Make sure we don't have a duplicate filename
            if (-e $self->{'path'}.'/'.$outfile.$suffix) {
                my $count = 1;
                my $out   = $outfile;
                while (-e $self->{'path'}.'/'.$out.$suffix) {
                    $count++;
                    $out = "$outfile.$count";
                }
                $outfile = $out;
           }
        # Store it so we don't have to recalculate this next time
            $episode->{'outfile'}{$suffix} = $outfile;
        }
    # return
        return $self->{'path'}.'/'.$outfile.$suffix;
    }

# A routine to grab resolutions
    sub query_resolution {
        my $self = shift;
    # Ask the user what resolution he/she wants
        if ($Args{'width'}) {
            die "Width must be > 0\n" unless ($Args{'width'} > 0);
            $self->{'width'} = $Args{'width'};
        }
        else {
            while (1) {
                my $w = query_text('Width?',
                                   'int',
                                   $self->{'width'});
            # Make sure this is a multiple of 16
                if ($w % 16 == 0) {
                    $self->{'width'} = $w;
                    last;
                }
            # Alert the user
                print "Width must be a multiple of 16.\n";
            }
        }
    # Height will default to whatever is the appropriate aspect ratio for the width
    # someday, we should check the aspect ratio here, too...  Round up/down as needed.
        $self->{'height'} = sprintf('%.0f', $self->{'width'} * 3/4);
        if ($self->{'height'} % 16 > 8) {
            while ($self->{'height'} % 16 > 0) {
                $self->{'height'}++;
            }
        }
        elsif ($self->{'height'} % 16 > 0) {
            while ($self->{'height'} % 16 > 0) {
                $self->{'height'}--;
            }
        }
    # Ask about the height
        if ($Args{'height'}) {
            die "Height must be > 0\n" unless ($Args{'height'} > 0);
            $self->{'height'} = $Args{'height'};
        }
        else {
            while (1) {
                my $h = query_text('Height?',
                                   'int',
                                   $self->{'height'});
            # Make sure this is a multiple of 16
                if ($h % 16 == 0) {
                    $self->{'height'} = $h;
                    last;
                }
            # Alert the user
                print "Height must be a multiple of 16.\n";
            }
        }
    }

# This subroutine forks and executes one system command - nothing fancy
    sub fork_command {
        my $command = shift;
        if ($DEBUG) {
            $command =~ s#\ 2>/dev/null##sg;
            print "\nforking:\n$command\n";
            return undef;
        }

    # Get read/write handles so we can communicate with the forked process
        my ($read, $write);
        pipe $read, $write;

    # Fork and return the child's pid
        my $pid = undef;
        if ($pid = fork) {
            close $write;
        # Return both the read handle and the pid?
            if (wantarray) {
                return ($pid, $read)
            }
        # Just the pid -- close the read handle
            else {
                close $read;
                return $pid;
            }
        }
    # $pid defined means that this is now the forked child
        elsif (defined $pid) {
            $is_child = 1;
            close $read;
        # Autoflush $write
            select((select($write), $|=1)[0]);
        # Run the requested command
            my ($data, $buffer) = ('', '');
            open(COM, "$command |") or die "couldn't run command:  $!\n$command\n";
            while (read(COM, $data, 10)) {
                next unless (length $data > 0);
            # Convert CR's to linefeeds so the data will flush properly
                $data =~ tr/\r/\n/s;
            # Some magic so that we only send whole lines (which helps us do
            # nonblocking reads on the other end)
                substr($data, 0, 0) = $buffer;
                $buffer  = '';
                if ($data !~ /\n$/) {
                    ($data, $buffer) = $data =~ /(.+\n)?([^\n]+)$/s;
                }
            # We have a line to print?
                if ($data && length $data > 0) {
                    print $write $data;
                }
            # Sleep for 1/100 second so we don't go too fast and annoy the cpu,
            # but still read fast enough that transcode won't slow down, either.
                usleep(5000);
            }
            close COM;
        # Print the return status of the child
            my $status = $? >> 8;
            print $write "!!! process $$ complete:  $status !!!\n";
        # Close the write handle
            close $write;
        # Exit using something that won't set off the END block
            POSIX::_exit($status);
        }
    # Couldn't fork, guess we have to quit
        die "Couldn't fork: $!\n\n$command\n\n";
    }

    sub has_data {
        my $fh = shift;
        my $r  = '';
        vec($r, fileno($fh), 1) = 1;
        my $can = select($r, undef, undef, 0);
        if ($can) {
            return vec($r, fileno($fh), 1);
        }
        return 0;
    }

    sub fifos_wait {
    # Sleep a bit to let mythtranscode start up
        my $fifodir = shift;
        my $overload = 0;
        if (!$DEBUG) {
            while (++$overload < 30 && !(-e "$fifodir/audout" && -e "$fifodir/vidout" )) {
                sleep 1;
                print "Waiting for mythtranscode to set up the fifos.\n";
            }
            unless (-e "$fifodir/audout" && -e "$fifodir/vidout") {
                die "Waited too long for mythtranscode to create its fifos.  Please try again.\n\n";
            }
        }
    }

# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
