#!/usr/bin/perl
use strict;
use warnings;
use Switch;
use IO::Socket;
use IO::Socket::Timeout;
use POSIX;
use String::Random qw(random_regex random_string);
use Errno qw(ETIMEDOUT EWOULDBLOCK);
use Time::HiRes qw(time);
use threads;
use threads::shared;
use String::Diff;
use Config;
$Config{useithreads} or
    die('Recompile Perl with threads to run this program.');

#my $remote_host='127.0.0.1';
#my $remote_port=5001;

my $remote_host=shift;
my $remote_port=shift;
my $fout=shift;
my $READTIMEOUT=3.0;
my $WORDCNT=2;


my  $text="Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua Ut enim ad minim veniam quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur Excepteur sint occaecat cupidatat non proident sunt in culpa qui officia deserunt mollit anim id est laborum";
my @textArray=split(/ /, $text);


open(FH, '>', "$fout") or die $!;

print "Starting client test, storing to $fout, sleeping 2s.\n";
#sleep(2);

my ($op,$v1,$v2,$remainder, $socket, $helloMsg,$assignment,$result,$response,$errorCount,$correctCount);

my $tests=2;
my $clientCount=0;
my %clients;

$errorCount=0;
my $errorLogString="";

my $msgCount :shared; 
my $sentMsg :shared;

$msgCount=0;
$sentMsg=0;

sub clientSub {
    my ($socket,$name,$id,$words)= @_;
    my $word;
    print "I am a THREAD, $name id=$id, strings=$words\n";
    my @stringStore=split(/ /,$words);
    my $m=$#stringStore;
    my $msgCounter=0;
    my $termCounter=0;
    my $k=0;
    while($termCounter==0){
	
	$word = $stringStore[$k];
	print $socket "MSG $word\n";
	print "TD[$name|$k/$m] ==================> MSG $word \n";
	while($word=<$socket>){
	    chomp($word);
	    if ( "$word" =~ /DROP/ ){
		$termCounter=1;
	    }
	    print "TD[$name|$k/$m] <= $word\n";
	}
	$k++;
	if($k>$#stringStore){
	    $k=0;
	}
    }
    
    
    return;
}
	
    

sub depart() {
    print "Im the departing subroutine, Errors: $errorCount.\n";
    print "$errorLogString\n";
    exit(1);
}

print "\nLaunching $tests additional clients.\n";
my $nickName;
my $haystackOfNames="";
my ($tim1,$tim2,$NICKstatus);
my %nickWordCombo;

for(my $i=0;$i<$tests;$i++) {
    $socket = IO::Socket::INET->new(PeerAddr => $remote_host,
				    PeerPort => $remote_port,
				    Proto    => "tcp",
				    Timeout  => 2,
				    Type     => SOCK_STREAM)
	or die "Couldn't connect to $remote_host:$remote_port : $@\n";
    
    $nickName="TC".random_regex('[A-Z]{2}[a-z]{2}')."$i";
    $haystackOfNames="$nickName $haystackOfNames";
    IO::Socket::Timeout->enable_timeouts_on($socket);
    $socket->read_timeout(3);
    $socket->write_timeout(0.5);
   
    $helloMsg = <$socket>;
    if ( $helloMsg =~ m[Hello 1\n]i ) {
	print ".";
    } elsif  ( $helloMsg =~ m[Hello 1.0\n]i ) {
	print ".";
    } else {
	print "Server said; |$helloMsg| not as expected.\n";
	$errorLogString="Incorrect connection for $nickName.\n$errorLogString";
	print $socket "ERROR\n";
	close($socket);
	depart();
	exit; 
    }
    #Send NICK
    
    print $socket "NICK $nickName\n";
    $tim1=time;
    $NICKstatus=<$socket>;
    $tim2=time;

    
    my $word = $textArray[ rand @textArray ];
    my $stringStore="";
    my $tmpKey;
    for(my $k=0;$k<$WORDCNT;$k++){
	$word = $textArray[ rand @textArray ];
	$tmpKey="$nickName$word";
	$nickWordCombo{$tmpKey}=1;
	$stringStore="$word $stringStore";
    }

    
    if ( $NICKstatus =~ m[OK\n]i ) {
	my %data=(
	    'sock' => $socket,
	    'nick' => "$nickName",
	    'id' => "$i",
	    'words' => "$stringStore",	    
	    );
	$clients{$nickName} = \%data;
	$clientCount++;
#	print $socket->sockhost() . ":" . $socket->sockport() ." --> " . $socket->peeraddr() . ":" . $socket->peerport ." ";
	print "Added $nickName with id=$i, took " . ($tim2-$tim1)*1000 ." [ms] \n";
    } else {
	print "Server rejected $nickName, took " . ($tim2-$tim1)*1000 . "[ms] \n";
	$errorLogString="Server rejected $nickName.\n$errorLogString";
	$errorCount++;
    }
}

print "\n";

##print "\nGot $clientCount accepted clients.\n\n";

my @clist=(keys %clients);
my $cNt=($#clist+1);
print "$cNt clients were successfully launched.\n\n";


my %thds;
#print "*****************\n";
#print "Starting threads.\n";
#print "*****************\n";

for(my $i=0;$i<$tests;$i++){
#    print "Threading " .$clients{$clist[$i]}{nick} ."/". $clients{$clist[$i]}{id} . " ";
    if ( $clients{$clist[$i]}{nick} eq "Bob" ) {

#	print "Not Bob\n";
    }else{
#	print ".\n";
	$thds{$clients{$clist[$i]}{nick}}=threads->create(\&clientSub,
							  $clients{$clist[$i]}{sock},
							  $clients{$clist[$i]}{nick},
							  $clients{$clist[$i]}{id},
							  $clients{$clist[$i]}{words});
    }
}
print "*****************\n";
print "Threads started.\n";
print "*****************\n\n";


my $myData;

print "Wait for threads to die.\n";
for(my $i=0;$i<$tests;$i++){
    $thds{$clients{$clist[$i]}{nick}}->join();
    print "Joining " .$clients{$clist[$i]}{nick} ."/". $clients{$clist[$i]}{id} . "\n";
}
print "After threads have died, we have msgCount=$msgCount.\n\n";



#print FH "$tests $correctCount $errorCount\n";
close(FH);


print "Closing server test script.\n";
depart();
exit;
