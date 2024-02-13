#include <iostream>
#include <map>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <curl/curl.h>
#include <string>
#include <cstring>
#include <cmath>
// #include <thread>

using namespace std;

static std::unordered_map<int, unordered_map<uint64_t, int64_t>> ds; // stream_id->tracker_id->last sent alert

const char* final_access_token = "";
static bool flag = false;

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

static int64_t time_diff = 0;
extern "C" {
    bool check_alert(int stream_id, uint64_t track_id, int64_t current_time) {
        
        if(flag==false && current_time>1){
            int64_t n_digits = floor(log10(current_time)); 
            int64_t power_of_10 = pow(10, n_digits);
            time_diff = current_time + power_of_10;
            flag=true;
        }
        auto it = ds.find(stream_id);
        if (it != ds.end()) {
            auto it2 = ds[stream_id].find(track_id);
            if (it2 != ds[stream_id].end()) {
                if (current_time - ds[stream_id][track_id] > time_diff)  {
                    // std::cout<<current_time<<"-"<<ds[stream_id][track_id]<<"="<<current_time - ds[stream_id][track_id]<<std::endl;
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







#include <opencv2/opencv.hpp>
#include <fstream>
#include <sstream>

// using namespace cv;
using namespace std;

void bbox_cv(){
    // Read the image
    cv::Mat image = cv::imread("/opt/nvidia/deepstream/deepstream-6.2/DeepStream-Yolo/download_frames/OSD_frame_8_0.jpg");
    if (image.empty()) {
        cerr << "Error: Could not read the image." << endl;
        return;
    }

    // Read annotation file
    ifstream annotationFile("/opt/nvidia/deepstream/deepstream-6.2/DeepStream-Yolo/detections/00_000_000008.txt");
    if (!annotationFile.is_open()) {
        cerr << "Error: Could not open the annotation file." << endl;
        return;
    }

    string line;
    while (std::getline(annotationFile, line)) {
        // Parse annotation
        std::istringstream iss(line);
        std::string className;
        iss >> className;

        std::vector<float> bbox;
        float coord;
        while (iss >> coord) {
            bbox.push_back(coord);
        }


        cv::rectangle(image, cv::Point(bbox[1], bbox[2]), cv::Point(bbox[5], bbox[6]), cv::Scalar(0, 255, 0), 2);

        cv::putText(image, className, cv::Point(bbox[1], bbox[2] - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
    }

    // Display and save the result
    cv::imshow("Image with Bounding Boxes", image);
    cv::imwrite("/opt/nvidia/deepstream/deepstream-6.2/DeepStream-Yolo/annotated_image.jpg", image);
    cv::waitKey(0);

    return;
}