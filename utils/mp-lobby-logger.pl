#!/usr/bin/perl

use strict;
use warnings;
use wml;
use IO::Socket;
use POSIX qw(strftime);
use Getopt::Std;
use Data::Dumper;

#my $usage = "Usage: $0 [-gjn] [-e count] [-l logfile] [-h [host]] [-p [port]] [-t [timestampformat]] [-u username]";

my %opts = ();
getopts('gjl:ne:h:p:t:u:',\%opts);

my $USERNAME = 'log';
$USERNAME    = $opts{'u'} if $opts{'u'};
my $HOST = '127.0.0.1';
$HOST    = 'server.wesnoth.org' if exists $opts{'h'};
$HOST    = $opts{'h'} if $opts{'h'};
my $PORT = '15000';
$PORT    = '14999' if exists $opts{'p'};
$PORT    = $opts{'p'} if $opts{'p'};
my $logtimestamp = "%Y%m%d %T ";
my $timestamp = "%Y%m%d %T ";
$timestamp    = $opts{'t'} if $opts{'t'};
$timestamp    = '' unless exists $opts{'t'};
my $showjoins = $opts{'j'};
my $showgames = $opts{'g'};
my $showturns = $opts{'n'};
my $logfile   = $opts{'l'};
if (exists $opts{'l'}) {open(LOG, ">> $logfile") or die "can't open $logfile: $!";}
select LOG; $| = 1;

my $LOGIN_RESPONSE = "[login]\nusername=\"$USERNAME\"\n[/login]";
my $VERSION_RESPONSE = "[version]\nversion=\"test\"\n[/version]";
my @users = ();
my @games = ();
my %outgoing_schemas = ();
my %incoming_schemas = ();


sub connect($$) {
	my ($host,$port) = @_;
	my $sock = IO::Socket::INET->new(PeerAddr => $host, PeerPort => $port, Proto => 'tcp', Type => SOCK_STREAM)
		or die "Could not connect to $host:$port: $@\n";
	print $sock pack('N',0) or die "Could not send initial handshake";

	my $connection_num = "";
	read $sock, $connection_num, 4;
	die "Could not read connection number" if length($connection_num) != 4;

	$outgoing_schemas{$sock} = [];
	$incoming_schemas{$sock} = [];
	return $sock;
}

sub disconnect($) {
	my ($sock) = @_;
	delete($outgoing_schemas{$sock});
	delete($incoming_schemas{$sock});
	close $sock;
}

sub read_packet($) {
	my ($sock) = @_;
	my $buf = '';
	read $sock, $buf, 4;
	die "Could not read length" if length($buf) != 4;

	my $len = unpack('N',$buf);

	my $res = "\0" * $len;
	my $count = 0;
	while($len > $count) {
		$buf = '';
		my $bytes = $len - $count;
		read $sock, $buf, $bytes or die "Error reading socket: $!";
		substr($res, $count, length $buf) = $buf;
		$count += length $buf;
	}

	$res = substr($res,0,$len-1);

	return &wml::read_binary($incoming_schemas{$sock},$res);
}

sub write_packet($$) {
	my ($sock,$doc) = @_;
	my $data = &wml::write_binary($outgoing_schemas{$sock},$doc);
	$data .= chr 0;
	my $header = pack('N',length $data);
	print $sock "$header$data" or die "Error writing to socket: $!";
}

sub write_bad_packet($$) {
	my ($sock, $doc) = @_;
	my $data = &wml::write_binary($outgoing_schemas{$sock},$doc);
	$data .= chr 0;
	my $header = pack('N', (length $data) * 2);
	print $sock "$header$data" or die "Error writing to socket: $!";
}


sub timestamp {
	return strftime($timestamp, localtime());
}

sub logtimestamp {
	return strftime($logtimestamp, localtime());
}

sub login($) {
	my $sock = shift;
	my $response = &read_packet($sock);
	# server asks for the version string or tells us to login right away
	if (&wml::has_child($response, 'version')) {
		&write_packet($sock, &wml::read_text($VERSION_RESPONSE));
		$response = &read_packet($sock);
		# server asks for a login
		if (&wml::has_child($response, 'mustlogin')) {
			&write_packet($sock, &wml::read_text($LOGIN_RESPONSE));
		} elsif (my $error = &wml::has_child($response, 'error')) {
			print STDERR "Error: $error->{'attr'}->{'message'}.\n" and die;
		} else {
			print STDERR "Error: Server didn't ask us to log in and gave no error.\n" . Dumper($response) and die;
		}
	} elsif (my $error = &wml::has_child($response, 'error')) {
		print STDERR "Error: $error->{'attr'}->{'message'}.\n" and die;
	} elsif (&wml::has_child($response, 'mustlogin')) {
		&write_packet($sock, &wml::read_text($LOGIN_RESPONSE));
	} else {
		print STDERR "Error: Server didn't ask for version or login and gave no error.\n" . Dumper($response) and die;
	}

	# server sends the join lobby response
	$response = &read_packet($sock);
	if (&wml::has_child($response, 'join_lobby')) {
	} elsif (my $error = &wml::has_child($response, 'error')) {
		print STDERR "Error: $error->{'attr'}->{'message'}.\n" and die;
	} else {
		print STDERR "Error: Server didn't ask us to join the lobby and gave no error.\n" . Dumper($response) and die;
	}

	# server sends the initial list of games and players
	$response = &read_packet($sock);
	#print STDERR Dumper($response);
	if (&wml::has_child($response, 'gamelist')) {
		foreach (@ {$response->{'children'}}) {
			if ($_->{'name'} eq 'gamelist') {
				@games = @ {$_->{'children'}};
			} elsif ($_->{'name'} eq 'user') {
				$users[@users] = $_;
			}
		}
	} elsif (my $error = &wml::has_child($response, 'error')) {
		print STDERR "Error: $error->{'attr'}->{'message'}.\n" and die;
	} else {
		print STDERR "Error: Server didn't send the initial gamelist and gave no error.\n" . Dumper($response) and die;
	}
	my $userlist = @users . " users:";
	foreach (@users) {
		$userlist .= " " . $_->{'attr'}->{'name'};
	}
	$userlist .= "\n";
	print STDERR $userlist if $showjoins;
	print LOG    $userlist if $logfile;
	my $gamelist = @games . " games:";
	foreach (@games) {
		$gamelist .= " \"" . $_->{'attr'}->{'name'} . "\"";
	}
	$gamelist .= "\n";
	print STDERR $gamelist if $showgames;
	print LOG    $gamelist if $logfile;
}

# make several connections and send packets with a wrong length then sleep indefinitely
if (my $count = $opts{'e'}) {
	for (1..$count) {
		my $socket = &connect($HOST,$PORT);
		&write_bad_packet($socket, &wml::read_text($VERSION_RESPONSE));
	}
	sleep();
}



print STDERR "Connecting to $HOST:$PORT as $USERNAME.\n";
print LOG    "Connecting to $HOST:$PORT as $USERNAME.\n" if $logfile;
my $socket = &connect($HOST,$PORT);
defined($socket) or die "Error: Can't connect to the server.";

login($socket);

while (1) {
	my $response = &read_packet($socket);
	foreach (@ {$response->{'children'}}) {
		if ($_->{'name'} eq 'message') {
			my $sender = $_->{'attr'}->{'sender'};
			my $message = $_->{'attr'}->{'message'};
			if ($message =~ s,^/me,,) {
				print STDERR &timestamp . "* $sender$message\n";
				print LOG &logtimestamp . "* $sender$message\n" if $logfile;
			} else {
				print STDERR &timestamp . "<$sender> $message\n";
				print LOG &logtimestamp . "<$sender> $message\n" if $logfile;
			}
		} elsif ($_->{'name'} eq 'whisper') {
			my $sender = $_->{'attr'}->{'sender'};
			my $message = $_->{'attr'}->{'message'};
			if ($message =~ s,^/me,,) {
				print STDERR &timestamp . "*$sender$message*\n";
				print LOG &logtimestamp . "*$sender$message*\n" if $logfile;
			} else {
				print STDERR &timestamp . "*$sender* $message\n";
				print LOG &logtimestamp . "*$sender* $message\n" if $logfile;
			}
		} elsif ($_->{'name'} eq 'gamelist_diff') {
			foreach (@ {$_->{'children'}}) {
				my $userindex = $_->{'attr'}->{'index'};
				if ($_->{'name'} eq 'insert_child') {
					if (my $user = &wml::has_child($_, 'user')) {
						my $username = $user->{'attr'}->{'name'};
						print STDERR &timestamp . "--> $username has logged on. ($userindex)\n" if $showjoins;
						print LOG &logtimestamp . "--> $username has logged on. ($userindex)\n" if $logfile;
						$users[@users] = $user;
					} else {
						print STDERR "[gamelist_diff]:" . Dumper($_);
					}
				} elsif ($_->{'name'} eq 'delete_child') {
					if (my $user = &wml::has_child($_, 'user')) {
						my $username = $users[$userindex]->{'attr'}->{'name'};
						print STDERR &timestamp . "<-- $username has logged off. ($userindex)\n" if $showjoins;
						print LOG &logtimestamp . "<-- $username has logged off. ($userindex)\n" if $logfile;
						splice(@users,$userindex,1);
					} else {
						print STDERR "[gamelist_diff]:" . Dumper($_);
					}
				} elsif ($_->{'name'} eq 'change_child') {
					if (my $user = &wml::has_child($_, 'user')) {
						foreach (@ {$user->{'children'}}) {
							#my $userindex = $_->{'attr'}->{'index'};   #the gamelistindex would be nice here.. probably hacky though so better put it in the 'location' key
							if ($_->{'name'} eq 'insert') {
								my $username = $users[$userindex]->{'attr'}->{'name'};
								if ($_->{'attr'}->{'available'} eq "yes") {
									my $location = $users[$userindex]->{'attr'}->{'location'};
									print STDERR &timestamp . "++> $username has left a game: \"$location\"\n" if $showjoins and $showgames;
									print LOG &logtimestamp . "++> $username has left a game: \"$location\"\n" if $logfile;
								} elsif ($_->{'attr'}->{'available'} eq "no") {
									my $location = $_->{'attr'}->{'location'};
									$users[$userindex]->{'attr'}->{'location'} = $location;
									print STDERR &timestamp . "<++ $username has joined a game: \"$location\"\n" if $showjoins and $showgames;
									print LOG &logtimestamp . "<++ $username has joined a game: \"$location\"\n" if $logfile;
								}
							} elsif ($_->{'name'} eq 'delete') {
							} else {
								print STDERR "[gamelist_diff][change_child][user]:" . Dumper($_);
							}
						}
						#print STDERR "[gamelist_diff][change_child]:" . Dumper($user);
					} elsif (my $gamelist = &wml::has_child($_, 'gamelist')) {
						foreach (@ {$gamelist->{'children'}}) {
							my $gamelistindex = $_->{'attr'}->{'index'};
							if ($_->{'name'} eq 'insert_child') {
								if (my $game = &wml::has_child($_, 'game')) {
									my $gamename = $game->{'attr'}->{'name'};
									my $gameid   = $game->{'attr'}->{'id'};
									my $era      = $game->{'attr'}->{'mp_era'};
									my $scenario = "unknown";   # some scenarios don't set the id
									$scenario    = $game->{'attr'}->{'mp_scenario'} if $game->{'attr'}->{'mp_scenario'};
									my $players  = $game->{'attr'}->{'human_sides'};
									my $xp       = $game->{'attr'}->{'experience_modifier'};
									my $gpv      = $game->{'attr'}->{'mp_village_gold'};
									print STDERR &timestamp . "+++ A new game has been created: \"$gamename\" ($gamelistindex, $gameid).\n" if $showgames;
									print LOG &logtimestamp . "+++ A new game has been created: \"$gamename\" ($gamelistindex, $gameid).\n" if $logfile;
									print STDERR &timestamp . "Settings: map: \"$scenario\"  era: \"$era\"  players: $players  XP: $xp  GPV: $gpv\n" if $showgames;
									print LOG &logtimestamp . "Settings: map: \"$scenario\"  era: \"$era\"  players: $players  XP: $xp  GPV: $gpv\n" if $logfile;
									$games[@games] = $game;
								} else {
									print "[gamelist_diff][change_child][gamelist]:" . Dumper($_);
								}
							} elsif ($_->{'name'} eq 'delete_child') {
								if (my $game = &wml::has_child($_, 'game')) {
									my $gamename = $games[$gamelistindex]->{'attr'}->{'name'};
									my $gameid   = $games[$gamelistindex]->{'attr'}->{'id'};
									my $turn     = $games[$gamelistindex]->{'attr'}->{'turn'};
									if (defined($turn)) {
										print STDERR &timestamp . "--- A game ended at turn $turn: \"$gamename\". ($gamelistindex, $gameid)\n" if $showgames;
										print LOG &logtimestamp . "--- A game ended at turn $turn: \"$gamename\". ($gamelistindex, $gameid)\n" if $logfile;
									} else {
										print STDERR &timestamp . "--- A game was aborted: \"$gamename\". ($gamelistindex, $gameid)\n" if $showgames;
										print LOG &logtimestamp . "--- A game was aborted: \"$gamename\". ($gamelistindex, $gameid)\n" if $logfile;
									}
									splice(@games,$gamelistindex,1);
								} else {
									print STDERR "[gamelist_diff][change_child][gamelist]:" . Dumper($_);
								}
							} elsif ($_->{'name'} eq 'change_child') {
								if (my $game = &wml::has_child($_, 'game')) {
									foreach (@ {$game->{'children'}}) {
										if ($_->{'name'} eq 'insert') {
											if (my $turn = $_->{'attr'}->{'turn'}) {
												my $gamename = $games[$gamelistindex]->{'attr'}->{'name'};
												my $gameid   = $games[$gamelistindex]->{'attr'}->{'id'};
												if ($turn =~ /1\//) {
													print STDERR &timestamp . "*** A game has started: \"$gamename\". ($gamelistindex, $gameid)\n" if $showgames;
													print LOG &logtimestamp . "*** A game has started: \"$gamename\". ($gamelistindex, $gameid)\n" if $logfile;
												} else {
													print STDERR &timestamp . "Turn changed to $turn in game: \"$gamename\". ($gamelistindex, $gameid)\n" if $showgames and $showturns;
													print LOG &logtimestamp . "Turn changed to $turn in game: \"$gamename\". ($gamelistindex, $gameid)\n" if $logfile;
												}
												$games[$gamelistindex]->{'attr'}->{'turn'} = $turn;
											}
										} elsif ($_->{'name'} eq 'delete') {
										} else {
											print "[gamelist_diff][change_child][gamelist][change_child][game]:" . Dumper($_);
										}
									}
								} else {
									print STDERR "[gamelist_diff][change_child][gamelist]:" . Dumper($_);
								}
							} else {
								print STDERR "[gamelist_diff][change_child][gamelist]:" . Dumper($_);
							}
						}
					} else {
						print STDERR "[gamelist_diff]:" . Dumper($_);
					}
				} else {
					print STDERR "[gamelist_diff]:" . Dumper($_);
				}
			}
		# [observer] and [observer_quit] should be deprecated they are redundant to parts of [gamelist_diff]
		} elsif ($_->{'name'} eq 'observer') {
#			my $username = $_->{'attr'}->{'name'};
#			print &timestamp . "++> $username has joined the lobby.\n";
		} elsif ($_->{'name'} eq 'observer_quit') {
#			my $username = $_->{'attr'}->{'name'};
#			print &timestamp . "<++ $username has left the lobby.\n";
		} else {
			print STDERR Dumper($_);
		}
	}

}
print "Connection closed.\n\n";
close(LOG);
