#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <curl/curl.h>
#include <string>
#include <cstring>
// #include <thread>

using namespace std;

static std::unordered_map<int, unordered_map<uint64_t, int64_t>> ds; // stream_id->tracker_id->last sent alert

const char* final_access_token = "";

size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *response) {
        response->append((char *)contents, size * nmemb);
        return size * nmemb;
    }

const char* getauthbear(const std::string& app_user, const std::string& app_pwd) {
    std::string loginurl = "https://app.invigilo.ai/api/v1/login/access-token";
    std::string data = "username=" + app_user + "&password=" + app_pwd;

    CURL *curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");

        curl_easy_setopt(curl, CURLOPT_URL, loginurl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    string access_token_str = response.substr(17, 147); //improve this (163)
    char* access_token = new char[access_token_str.size() + 1];
    std::copy(access_token_str.begin(), access_token_str.end(), access_token);
    access_token[access_token_str.size()] = '\0';
    return access_token;
}


extern "C" {
    bool check_alert(int stream_id, uint64_t track_id, int64_t current_time) {
        auto it = ds.find(stream_id);
        if (it != ds.end()) {
            auto it2 = ds[stream_id].find(track_id);
            if (it2 != ds[stream_id].end()) {
                if (current_time - ds[stream_id][track_id] > 50000000)  {
                    return true;
                } else {
                    return false;
                }
            } else {
                ds[stream_id][track_id] = 0;
                return true;
            }
        } else {
            // Add new stream_id into ds
            ds[stream_id] = std::unordered_map<uint64_t, int64_t>();
            return true;
        }
        return false;
    }

    bool send_alert();

    const char* getaccesstoken(){
        if (final_access_token == ""){
            const char* username = "ananda.hexacon.2@invigilo.sg";
            const char* password = "1y2u3i4o5P!()";
            final_access_token = getauthbear(strdup(username), strdup(password));

            // final_access_token = getauthbear("ananda.hexacon.2@invigilo.sg", "1y2u3i4o5P!()");
        }
        return final_access_token;
    }

}






