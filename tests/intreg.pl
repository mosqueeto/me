#!/usr/bin/perl -T
use Socket;
use Fcntl qw(:DEFAULT :flock);
use POSIX;

#
#   web forms that generate templates for creation and modification of
#   .int names.
#

$ENV{PATH} = "";
$bgcolor="ccccff";                  # used in html templates
$calling_url = $ENV{SCRIPT_NAME};   # used in html templates
#$registrar='iana@iana.org';
$OK_CHARS='-a-zA-Z0-9.';
&openlog("/tmp/intlog");
$today = localtime;

&init();

%in = &getcgivars ;
&logit("domain=$in{'domain'}, action=$in{'action'}\n");

&new_dom_input()   if( $in{'action'} eq "new" );
&init_page()       if( !$in{'domain'} );
&process_new_dom() if( $in{'action'} eq "process_new_dom" );
&mod_dom_input()   if( $in{'action'} eq "modify" );
&process_mod_dom() if( $in{'action'} eq "process_mod_dom" );

# we should never get here...

&htmldie("We are sorry, your form cannot be processed: [$in{'action'}]");

exit;

################################################################
################################################################

sub process_new_dom {

    unless( &good_dn($in{'domain'}) ) {
        &htmldie(
            "Sorry, $in{'domain'} is not a legal domain name.",
            "Illegal Domain Name");
    } 
    my $wdata = &get_whois($in{domain});
    # already exists?
    unless( $wdata =~ /Domain .* not found./ )  {
        &htmldie(
            "Sorry, that name is already in use:\n\n<pre>$wdata</pre>",
            "Name in Use");
    }

    &chk_info();

#
#   All OK -- mail output to admin
#


    $src = "$in{'Admin Email'}";
    $src .= ",$in{'Tech Email'}" if $in{"Tech Email"} ne $in{"Admin Email"};
    &mail_info($src,"Create New Domain $in{domain}");

    &html_head("Thank You");
    print <<EOF;
<center><font size=+2>
Thank you.  You will receive a confirmation of receipt of your
application/modification template within 7 working days.
</font>
</center>
EOF
    &html_close();
    &logit("template accepted\n");
    exit;

}
################################################################

sub process_mod_dom {

    unless( &good_dn($in{'domain'}) ) {
        &htmldie(
            "Sorry, $in{'domain'} is not a legal domain name.",
            "Illegal Domain Name");
    } 
    my $wdata = &get_whois($in{domain});
    # already exists?
    if( $wdata =~ /Domain $in{'domain'} not found./ )  {
        &htmldie(
            "Sorry, that name is not registered.",
            "Name Not Registered");
    }

    &chk_info();

    $src = "$in{'Admin Email'}";
    $src .= ",$in{'Tech Email'}" if $in{"Tech Email"} ne $in{"Admin Email"};

#   flag entries that are different from the whois data
    foreach $k (keys %in) {
        $modified{$k} = ' ';
        next if $k eq "domain";
        next if $k eq 'Supplemental Information';
        if( $in{$k} || $whois{$k} ) {
            $in{$k} =~ s/^\s+//;
            $in{$k} =~ s/\s+$//;
            $in{$k} =~ s/\s+$/ /;
            $whois{$k} =~ s/\s+$//;
            $whois{$k} =~ s/\s+$//;
            $whois{$k} =~ s/\s+$/ /;
            if( $whois{$k} and ($in{$k} ne $whois{$k}) ) {
                $in{$k} = "$in{$k} (was: \"$whois{$k}\")";
                $modified{$k} = '*';
            }
        }
    }
#
#   All OK -- mail output to admin
#

    &mail_info($src,"Modify Domain $in{domain}");

    &html_head("Thank You");
    print <<EOF;
<center><font size=+2>
Thank You.  You will receive a confirmation of receipt of your
application/modification template within 7 working days.
</font>
</center>
EOF
    &html_close();
    &logit("template accepted\n");
    exit;

}
################################################################

sub chk_info {

#
#   check registrant
#
    $errmsg = "";
    foreach $n (keys %domain_required) {
        if( ! $in{$n} ) {
            $errmsg .= "<li>$domain_required{$n}\n";
        }
    }
    if( $errmsg ) {
        $errmsg = "The following required fields in the registrant " .
            "were not filled out<br>\n<ul>\n$errmsg</ul>\n";
    }

#
#   check admin contact info
#
    $admerr = "";   
    foreach $n (keys %admin_required) {
      if( ! $in{$n} ) {
          $admerr .= "<li>$admin_required{$n}\n";
      }
    }
    if( $admerr )  {
        $errmsg .= "<p>The following required fields in the Admin Contact".
                 " section are missing:<br>\n<ul>$admerr</ul>\n";
    }

#
#   check tech contact
#
    $techerr = "";
    if( $in{'same_as_admin'} eq "yes" ) {
        foreach $n (keys %in) {
            if( $n =~ /^Admin (.*)$/ ) {
                $in{"Tech $1"} = $in{$n};
            }
        }
    }
    else {
        foreach $n (keys %tech_required) {
          if( ! $in{$n} ) {
              $techerr .= "<li>$tech_required{$n}\n";
          }
        }
    }
    if( $techerr )  {
        $errmsg .= "<p>The following required fields in the Tech Contact".
                   " section are missing:<br>\n<ul>$techerr</ul>\n";
    }

#
#   check name servers
#
    $nserr = "";
    foreach $n (keys %nameserver_required) {
       if( ! $in{$n} ) {
          $nserr .= "<li>$nameserver_required{$n}\n";
        }
    }
    if( $nserr )  {
        $errmsg .= "<p>You have not supplied information for the ".
                "required two nameservers.  We are missing:<br>".
                "\n<ul>$nserr</ul>\n";
    }

    for( $i=0; $i<8; $i++ ) {
      $ns_name = "Nameserver".$i;
      $ns_addr = "IPAddr".$i;
      if( $in{$ns_name} or $in{$ns_addr} ) {
        if( !$in{$ns_name} ) {
            $errmsg .= 
            "<p>Nameserver $i: Missing name for secondary NS $in{$ns_addr}.";
        }
        if( !$in{$ns_addr} ) {
            $errmsg .= 
          "<p>Nameserver $i: Missing address for secondary NS $in{$ns_name}.";
        }
        if( ! &good_ip($in{$ns_addr}) ) {
             $errmsg .= 
            "<p>Nameserver $i: $in{$ns_addr} is not a legal IP address.";
        }
        if( ! &good_dn($in{$ns_name}) ) {
             $errmsg .= 
             "Nameserver $i: <p>$in{$ns_name} is not a legal domain name.";
        }
      }
    }

    if( $errmsg ) {
        &htmldie("$errmsg");
    }
}

################################################################

sub mail_info {

my ($src,$subj) = @_;

my $date = localtime;
my $msg = <<EOF;

We have received the following template at the IANA.  You have been sent
a copy for one of the following reasons:

1. You are listed as the administrative contact
2. You are listed as the technical contact

This copy is sent only for your information.  However, if any of the
information on the template below is incorrect or you feel there is a
mistake in you being listed as a contact person, please let us know. 

Thank you,

.int Domain Registry
IANA - Internet Assigned Numbers Authority

================================================================

Domain Name:         $in{'domain'}

Registrant:
    $modified{"Registrant Name"}Organization Name: 
    $in{"Registrant Name"}
    $modified{"Registrant Address1"}Address1:       $in{"Registrant Address1"}
    $modified{"Registrant Address2"}Address2:       $in{"Registrant Address2"}
    $modified{"Registrant Address3"}Address3:       $in{"Registrant Address3"}
    $modified{"Registrant City"}City:           $in{"Registrant City"}
    $modified{"Registrant State/Province"}State/Province: $in{"Registrant State/Province"}
    $modified{"Registrant Country"}Country:        $in{"Registrant Country"}
    $modified{"Registrant Postal Code"}Postal Code:    $in{"Registrant Postal Code"}
    
Admin:
    $modified{"Admin Name"}Name:             $in{"Admin Name"}
    $modified{"Admin Address1"}Address1:         $in{"Admin Address1"}
    $modified{"Admin Address2"}Address2:         $in{"Admin Address2"}
    $modified{"Admin Address3"}Address3:         $in{"Admin Address3"}
    $modified{"Admin City"}City:             $in{"Admin City"}
    $modified{"Admin State/Province"}State/Province:   $in{"Admin State/Province"}
    $modified{"Admin Country"}Country:          $in{"Admin Country"}
    $modified{"Admin Postal Code"}Postal Code:      $in{"Admin Postal Code"}
    $modified{"Admin Phone"}Phone:            $in{"Admin Phone"}
    $modified{"Admin Fax"}Fax:              $in{"Admin Fax"}
    $modified{"Admin Email"}Email:            $in{"Admin Email"}
    
Tech:
    $modified{"Tech Name"}Name:              $in{"Tech Name"}
    $modified{"Tech Address1"}Address1:          $in{"Tech Address1"}
    $modified{"Tech Address2"}Address2:          $in{"Tech Address2"}
    $modified{"Tech Address3"}Address3:          $in{"Tech Address3"}
    $modified{"Tech City"}City:              $in{"Tech City"}
    $modified{"Tech State/Province"}State/Province:    $in{"Tech State/Province"}
    $modified{"Tech Country"}Country:           $in{"Tech Country"}
    $modified{"Tech Postal Code"}Postal Code:       $in{"Tech Postal Code"}
    $modified{"Tech Phone"}Phone:             $in{"Tech Phone"}
    $modified{"Tech Fax"}Fax:               $in{"Tech Fax"}
    $modified{"Tech Email"}Email:             $in{"Tech Email"}
    
Nameserver Info:
    $modified{"Nameserver1"}Nameserver1:          $in{"Nameserver1"}
    $modified{"IPAddr1"}IPAddr1:              $in{"IPAddr1"}
    $modified{"Nameserver2"}Nameserver2:          $in{"Nameserver2"}
    $modified{"IPAddr2"}IPAddr2:              $in{"IPAddr2"}
    $modified{"Nameserver3"}Nameserver3:          $in{"Nameserver3"}
    $modified{"IPAddr3"}IPAddr3:              $in{"IPAddr3"}
    $modified{"Nameserver4"}Nameserver4:          $in{"Nameserver4"}
    $modified{"IPAddr4"}IPAddr4:              $in{"IPAddr4"}
    $modified{"Nameserver5"}Nameserver5:          $in{"Nameserver5"}
    $modified{"IPAddr5"}IPAddr5:              $in{"IPAddr5"}
    $modified{"Nameserver6"}Nameserver6:          $in{"Nameserver6"}
    $modified{"IPAddr6"}IPAddr6:              $in{"IPAddr6"}

Supplemental Info:
$in{'Supplemental Information'}


    * indicates modified information

EOF
#
#   Send a copy to the admin and tech contacts, from iana
#
&send_mail( $src,
            'int-dom@iana.org',
            $subj,$msg);
#
#   send a copy to IANA, from the contacts
#
&send_mail( 'int-dom@iana.org',
            $src,
            $subj,$msg);

}

################################################################
# Read all CGI vars into an associative array.
# If multiple input fields have the same name, they are concatenated into
#   one array element and delimited with the \0 character.
# Currently only supports Content-Type of application/x-www-form-urlencoded.
sub getcgivars {
    local($in, %in) ;
    local($name, $value) ;

    # First, read entire string of CGI vars into $in
    if ($ENV{'REQUEST_METHOD'} eq 'GET') {
        $in= $ENV{'QUERY_STRING'} ;
    } elsif ($ENV{'REQUEST_METHOD'} eq 'POST') {
         if ($ENV{'CONTENT_TYPE'}=~ m#^application/x-www-form-urlencoded$#i) {
            $ENV{'CONTENT_LENGTH'}
                || &htmldie("No Content-Length sent with the POST request.") ;
            read(STDIN, $in, $ENV{'CONTENT_LENGTH'}) ;
        } else {
            &htmldie("Unsupported Content-Type: $ENV{'CONTENT_TYPE'}") ;
        }
    } else {
        &htmldie("Script was called with unsupported REQUEST_METHOD.") ;
    }

    # Resolve and unencode name/value pairs into %in
    foreach (split('&', $in)) {
        s/\+/ /g ;   
        ($name, $value)= split('=', $_, 2) ;
        $name=~ s/%(..)/sprintf("%c",hex($1))/ge ;
        $value=~ s/%(..)/sprintf("%c",hex($1))/ge ; 
#        $value=~ s/$BAD_CHARS/-/g;
#        $in{$name}.= "\0" if defined($in{$name}) ;
#        $in{$name}.= "$value\t" ; #separate with tab
        $in{$name}.= $value;
    }
    return %in ;
}

################################################################            
        
# Die, outputting HTML error page
# If no $title, use a default title
sub htmldie {
    local($msg,$title)= @_ ;
    $title || ($title= "Error:") ;

&logit("htmldie: message:\n$msg\n");
&html_head("$title");
print "<center><table width=\"80%\"><tr><td>\n$msg\n</td></tr></table></center>\n";
&html_close();


    exit;
}


################################################################

sub new_dom_input {

    if( ! &good_dn($in{'domain'}) ) {
        &htmldie("Sorry, $in{'domain'} is not a legal domain name\n");
    }

    my $wdata = &get_whois($in{'domain'});
    unless( $wdata =~ /Domain .* not found/ )  {
        &htmldie(
            "Sorry, that name is already in use:\n\n<pre>$wdata</pre>",
            "Name in Use");
    }

    &display_tmplt("./createint.html");

}

################################################################

sub mod_dom_input {

    if( ! &good_dn($in{'domain'}) ) {
        &htmldie("Sorry, $in{'domain'} is not a legal domain name\n");
    }

    my $wdata = &get_whois($in{'domain'});
    if( $wdata =~ /Domain .+ not found/ )  {
        &htmldie(
            "Sorry, that name ($in{domain}) is not found in whois.",
            "Name Not Found");
    }
    # see if the tech contact info is identical to the admin info
    $tech_same_as_admin = "checked";
    foreach (keys %whois) {
        next if $_ !~ /^Admin /;
        my ($root) = /^Admin (.*)$/;
        if ( $whois{"Admin $root"} ne $whois{"Tech $root"} ) {
            $tech_same_as_admin = "";
            last;
        }
    }
#    if( $tech_same_as_admin ) {
#        foreach (keys %whois) {
#            if( $_ =~ /^Tech / ) {
#                $whois{$_} = "";
#            }
#        }
#    }

    &display_tmplt("./modifyint.html");
}

################################################################

sub init_page {
    &display_tmplt("./init.html");
    exit;
}
################################################################

sub get_whois
{
    my $query = $_[0];
 
    my $addr = gethostbyname('whois.iana.org');   
    my $dest = sockaddr_in(43,$addr);
    socket(SOCK,PF_INET,SOCK_STREAM,6);
    connect(SOCK,$dest);
    select((select(SOCK),$| = 1)[0]);
    print SOCK "$query\r\n";
    @whois_result = <SOCK>;
    close SOCK;
    my $result = join "", (@whois_result);
#    &logit($result);

#   scan the whois info, one line at a time, and generate form variables.
#   the whois data is in the form <lable>: <data>
    my $prefix = "";
    foreach (@whois_result) {

        # clean the lines; eliminate blank lines
        s/^\s*//;
        s/\s*$//;
        next if /^$/ ;

        # isolate the section prefix
        if( /^Registrant:$/ ) {
            $prefix = "Registrant ";
            next;
        }
        if( /^Administrative Contact:$/ ) {
            $prefix = "Admin ";
            next;
        }
        if( /^Technical Contact:$/ ) {
            $prefix = "Tech ";
            next;
        }
        if( /^Nameserver Information:$/ ) {
            $prefix="";
            next;
        }

  
        # build up the "whois" hash
        if( /^Nameserver: (.*)$/ ) {
            $nsidx++;
            $id = "Nameserver" . $nsidx;
            $whois{$id} = $1;
            next;
        }
        if( /^IP Address: (.*)$/ ) {
            $id = "IPAddr" . $nsidx;
            $whois{$id} = $1;
            next;
        }

        # build the variable name
        if( m|^([-a-zA-Z0-9 /]+): (.*)$| ) {
            $id = $prefix . $1;
            $whois{$id} = $2;
        }
        next;
    }
    return $result;
}

################################################################
sub good_ip {
    my ($ip) = @_;
    my ($o1,$o2,$o3,$o4) = split /\./,$ip;
    return 0 if $o1 !~ m/^\d{1,3}$/;
    return 0 if $o2 !~ m/^\d{1,3}$/;
    return 0 if $o3 !~ m/^\d{1,3}$/;
    return 0 if $o4 !~ m/^\d{1,3}$/;
    return 0 if ($o1 > 255) or ($o2 > 255) or ($o3 > 255) or ($o4 > 255);
    return 1;
}

sub good_dn {
    my ($dn) = @_;
    my @links = split /\./,$dn;
    my $l;
    if ( @links < 1 ) {  #####working here (kc)
        return 0;
    }
    foreach $l ( @links ) {
         return 0 if lc($l) !~ m/^[-a-z0-9]+$/;
    }
    return 1;
}

sub logit {
    my ($m)=@_;
    if( ! flock(LOG, LOCK_EX) ) {
       &htmldie("Can't lock logfile [$logfile]: $!");
    }
    seek(LOG,0,2);
    my $d = localtime;
    print LOG "\n$d: remote: $ENV{REMOTE_ADDR}; $m";
    if( ! flock(LOG, LOCK_UN) ) {
       &htmldie("Can't unlock logfile [$logfile]: $!");
    }
}

sub openlog {
    my ($logfile) = @_;
    if( ! sysopen LOG,"$logfile", O_RDWR|O_CREAT ) {
            &htmldie("$today: Can't open [$logfile]: $!");
    }
    # make it unbuffered...
    select((select(LOG),$|=1)[0]);
}
################################################################
sub send_mail {
    my ($to,$from,$subject,$msg) = @_;

    my $today = localtime;
    if( 1 ) {
#        open (MAIL, "| /usr/sbin/sendmail -oi -oem -f $from $to");
        open (MAIL, "| /usr/sbin/sendmail -t -n -oi -oi");
    }
    else {
        open (MAIL, ">>/tmp/testmail");
    }   
    print MAIL <<EOF;
From: $from
To: $to
Date: $today
Subject: $subject

$msg

EOF
    close (MAIL);
}
################################################################

sub display_tmplt {

    my $fname = shift;
    open TMPLT, "<$fname" or &htmldie("couldn't open template $fname: $!");

    print "Content-type: text/html\n\n";
    while( <TMPLT> ) {
        s|(\$[a-zA-Z0-9_{}' /]+)|$1|eeg;
        print;
    }   
    close TMPLT;
    exit;
}
    
################################################################
    
sub html_close {
print <<EOF;
<center><hr noshade>
<font face="arial,helvetica,Sans-Serif"><small>
Please send comments on this web site to:
<a href="mailto:webmaster\@iana.org">webmaster\@iana.org</a><br>
Page Updated 09-Sep-2000.<br>
&copy; 1998-2000&nbsp; The Internet Assigned
Numbers Authority. All rights reserved.
</small></font>
</center>
<!-- margins --></td></tr></table></center>
</body>
</html>
EOF
}

################################################################
sub html_head {
my ($title,$bgcolor) = @_;
$bgcolor || ($bgcolor = "ffffff");
print <<EOF;
Content-Type: text/html; charset=ISO-8859-1

<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<title>$title</title> 
</head>

<body bgcolor="$bgcolor">
<!-- margins -->
<center><table width="90%"><tr><td>

<table width="100%" border=0>
<tr>
  <td>
    <center>
    <img src="/logos/iana1.gif"
      width="407" height="148" naturalsizeflag="0" align="top">
    </center>
  </td>
  </tr>
  <tr>
  <td>
    <center><font size="+2" face="Sans-Serif">
        <h3>$title</h3>
    </font></center>
   </td>
</tr>
</table>
</center>

<center><hr noshade></center></p>

EOF
}

sub init {
%domain_required = (
"Registrant Name" => "Registrant Name",
"Registrant Address1" => "Registrant Address 1",
"Registrant City" => "Registrant City",
"Registrant Country" => "Registrant Country"
);

%admin_required = (
"Admin Name" => "Admin Contact Name",
"Admin Address1"  => "Admin Contact Address 1",
"Admin City"  => "Admin Contact City",
"Admin Country"  => "Admin Contact Country",
"Admin Phone"  => "Admin Contact Phone",
"Admin Email"  => "Admin Contact Email",
);

%tech_required = (
"Tech Name" => "Tech Contact Name",
"Tech Address1"  => "Tech Contact Address 1",
"Tech City"  => "Tech Contact City",
"Tech Country"  => "Tech Contact Country",
"Tech Phone"  => "Tech Contact Phone",
"Tech Email"  => "Tech Contact Email",
);

%nameserver_required = (
"Nameserver1" => "Primary Name Server Domain Name",
"IPAddr1" => "Primary Name Server IP Address",
"Nameserver2"  => "Secondary Name Server Domain Name",
"IPAddr2"  => "Secondary Name Server IP Address"
);

}
