#!/usr/bin/perl
#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
# $Id: wpsbuild.pl 24813 2010-02-21 19:10:57Z kugel $
#

use strict;
use Getopt::Long qw(:config pass_through);	# pass_through so not confused by -DTYPE_STUFF

my $ROOT="..";
my $verbose;
my $rbdir=".rockbox";
my $wpslist;
my $target;
my $modelname;

# Get options
GetOptions ( 'r|root=s'		=> \$ROOT,
	     'm|modelname=s'	=> \$modelname,
	     'v|verbose'	=> \$verbose,
	     'rbdir=s'          => \$rbdir, # If we want to put in a different directory
    );

($wpslist, $target) = @ARGV;

my $firmdir="$ROOT/firmware";
my $cppdef = $target;
my @depthlist = ( 16, 8, 4, 2, 1 );


# LCD sizes
my ($main_height, $main_width, $main_depth);
my ($remote_height, $remote_width, $remote_depth);
my $has_remote;


if(!$wpslist) {
    print "Usage: wpsbuilds.pl <WPSLIST> <target>\n",
    "Run this script in the root of the target build, and it will put all the\n",
    "stuff in $rbdir/wps/\n";
    exit;
}

sub getlcdsizes
{
    my ($remote) = @_;

    open(GCC, ">gcctemp");
    if($remote) {
        # Get the remote LCD screen size
    print GCC <<STOP
\#include "config.h"
#ifdef HAVE_REMOTE_LCD
Height: LCD_REMOTE_HEIGHT
Width: LCD_REMOTE_WIDTH
Depth: LCD_REMOTE_DEPTH
#endif
STOP
;
    }
    else {
    print GCC <<STOP
\#include "config.h"
Height: LCD_HEIGHT
Width: LCD_WIDTH
Depth: LCD_DEPTH
STOP
;
}
    close(GCC);

    my $c="cat gcctemp | gcc $cppdef -I. -I$firmdir/export -E -P -";

    #print "CMD $c\n";

    open(GETSIZE, "$c|");

    my ($height, $width, $depth);
    while(<GETSIZE>) {
        if($_ =~ /^Height: (\d*)/) {
            $height = $1;
        }
        elsif($_ =~ /^Width: (\d*)/) {
            $width = $1;
        }
        elsif($_ =~ /^Depth: (\d*)/) {
            $depth = $1;
        }
        if($height && $width && $depth) {
            last;
        }
    }
    close(GETSIZE);
    unlink("gcctemp");

    return ($height, $width, $depth);
}

# Get the LCD sizes first
($main_height, $main_width, $main_depth) = (320,240,16);#getlcdsizes();
($remote_height, $remote_width, $remote_depth) = ();#getlcdsizes(1);

#print "LCD: ${main_width}x${main_height}x${main_depth}\n";
$has_remote = 1 if ($remote_height && $remote_width && $remote_depth);

my $isrwps;
my $within;

my %theme;


sub match {
    my ($string, $pattern)=@_;

    $pattern =~ s/\*/.*/g;
    $pattern =~ s/\?/./g;

    return ($string =~ /^$pattern\z/);
}

sub matchdisplaystring {
    my ($string)=@_;
    return ($string =~ /${main_width}x${main_height}x${main_depth}/i) || 
            ($string =~ /r${remote_width}x${remote_height}x${remote_depth}/i);
}

sub mkdirs
{
    my ($themename) = @_;
    mkdir "$rbdir", 0777;
    mkdir "$rbdir/wps", 0777;
    mkdir "$rbdir/themes", 0777;

    if( -d "$rbdir/wps/$themename") {
  #      print STDERR "wpsbuild warning: directory wps/$themename already exists!\n";
    }
    else
    {
       mkdir "$rbdir/wps/$themename", 0777;
    }
}

sub buildcfg {
    my ($themename) = @_;
    my @out;    

    push @out, <<MOO
\#
\# generated by wpsbuild.pl
\# $themename is made by $theme{"Author"}
\#
MOO
;
    my %configs = (
                "Name" => "# Theme Name",
                "WPS" => "wps", "RWPS" => "rwps",
                "FMS" => "fms", "RFMS" => "rfms",
                "SBS" => "sbs", "RSBS" => "rsbs",
                "Font" => "font", "Remote Font" => "remote font",
                "Statusbar" => "statusbar", "Remote Statusbar" => "remote statusbar",
                "selector type" => "selector type", "Remote Selector Type" => "remote selector type",  
                "backdrop" => "backdrop", "iconset" => "iconset", "viewers iconset" => "viewers iconset",
                "remote iconset" => "remote iconset", "remote viewers iconset" => "remote viewers iconset",
                "Foreground Color" => "foreground color", "Background Color" => "background color", 
                "backdrop" => "backdrop"
                );
                
    while( my ($k, $v) = each %configs )
    {
        if ($k =~ "Name")
        {
            # do nothing
        }
        elsif ($k =~ "/WPS|RWPS|FMS|RFMF|SBS|RSBS/" && exists($theme{$k}))
        {
            push (@out, "$v: $themename.$v\n");
        }
        elsif (exists($theme{$k}))
        {
            push (@out, "$v: $theme{$k}\n");
        }
    }

  #  if(-f "$rbdir/themes/$themename.cfg") {
  #      print STDERR "wpsbuild warning: $themename.cfg already exists!\n";
  #  }
  #  else {
        open(CFG, ">$rbdir/themes/$themename.cfg");
        print CFG @out;
        close(CFG);
  #  }
}

open(WPS, "<$wpslist");
while(<WPS>) {
    my $l = $_;
    
    # remove CR
    $l =~ s/\r//g;
    if($l =~ /^ *\#/) {
        # skip comment
        next;
    }
    if($l =~ /^ *<(r|)wps>/i) {
        $isrwps = $1;
        $within = 1;
        undef %theme;
        next;
    }
    if($within) {
        if($l =~ /^ *<\/${isrwps}wps>/i) {
            #get the skin directory
            $wpslist =~ /(.*)WPSLIST/;
            my $wpsdir = $1;
            $within = 0;
            
            next if (!exists($theme{Name}));
            mkdirs($theme{Name});
            buildcfg($theme{Name});
            
            
            
            
            
            
        }
        elsif($l =~ /^([\w ]*)\.?(.*):\s*(.*)/) {
            my $var = $1;
            my $extra = $2;
            my $value = $3;
            if (!exists($theme{$var}))
            {
                if (!$extra || 
                    ($extra && (match($target, $extra) || matchdisplaystring($extra))))
                {
                    $theme{$var} = $value;
                  #  print "\'$var\': $value\n";
                }
            }
        }
        else{
            #print "Unknown line:  $l!\n";
        }
    }
}

close(WPS);
