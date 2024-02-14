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
#include <thread>
#include <fstream>
#include <sstream>
#include <mutex>
#include <opencv2/opencv.hpp>
// #include <aws/core/Aws.h>
// #include <aws/s3/S3Client.h>
// #include <aws/s3/model/PutObjectRequest.h>

using namespace std;

static std::unordered_map<int, unordered_map<uint64_t, int64_t>> ds; // stream_id->tracker_id->last sent alert

const char* final_access_token = "";
static bool flag = false;

size_t write_call_back(void *contents, size_t size, size_t nmemb, std::string *response) {
        response->append((char *)contents, size * nmemb);
        return size * nmemb;
    }

const char* get_auth_bear(const std::string& app_user, const std::string& app_pwd) {
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
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_call_back);
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

void perform_curl_request(const std::string& url, const std::string& json_data, const std::string& server_access_token) {
    CURL *curl = curl_easy_init();

    if (curl) {
        CURLcode res;
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json, text/plain, */*");
        headers = curl_slist_append(headers, ("Access-Token: " + server_access_token).c_str());
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());

        // Enable automatic redirection
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        // Capture response data
        std::string response_data;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_call_back);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        // Perform the request
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;

        // Get HTTP status code
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        std::cout << "HTTP Status Code: " << http_code << std::endl;
        std::cout << "Response Data: " << response_data << std::endl;

        // Cleanup
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    else {
        std:cerr << "CURL failed to init." << std::endl;
    }
}




// // Mutex for thread-safe output
// std::mutex output_mutex;

// void upload_image_to_S3(const std::string& access_key_id, const std::string& secret_access_key,
//                      const std::string& bucket_name, const std::string& file_path,
//                      const std::string& s3_key) {

//     Aws::SDKOptions options;
//     Aws::InitAPI(options);
//     Aws::Auth::AWSCredentials credentials(access_key_id, secret_access_key);
//     Aws::S3::S3Client s3_client(credentials, Aws::Client::ClientConfiguration());

//     std::ifstream fileStream(file_path.c_str(), std::ios::in | std::ios::binary);
//     if (!fileStream) {
//         std::lock_guard<std::mutex> lock(output_mutex);
//         std::cerr << "Failed to open file " << file_path << std::endl;
//         return;
//     }

//     fileStream.seekg(0, std::ios::end);
//     const auto file_length = fileStream.tellg();
//     fileStream.seekg(0, std::ios::beg);

//     std::vector<char> buffer(file_length);
//     fileStream.read(buffer.data(), file_length);

//     Aws::S3::Model::PutObjectRequest object_request;
//     object_request.WithBucket(bucket_name).WithKey(s3_key).WithBody(Aws::MakeShared<Aws::StringStream>(""), Aws::String(buffer.data(), buffer.size()));

//     auto put_object_outcome = s3_client.PutObject(object_request);
//     if (put_object_outcome.IsSuccess()) {
//         std::lock_guard<std::mutex> lock(output_mutex);
//         std::cout << "Successfully uploaded image " << file_path << " to S3 bucket" << std::endl;
//     } else {
//         std::lock_guard<std::mutex> lock(output_mutex);
//         std::cerr << "Failed to upload image " << file_path << " to S3 bucket: " << put_object_outcome.GetError().GetMessage() << std::endl;
//     }
//     Aws::ShutdownAPI(options);
// }


std::mutex mtx; // Mutex to protect shared data

void process_annotation(const std::string& line, cv::Mat& image) {
    std::istringstream iss(line);
    std::string className;
    iss >> className;

    std::vector<float> bbox;
    float coord;
    while (iss >> coord) {
        bbox.push_back(coord);
    }

    // Draw bounding box
    cv::rectangle(image, cv::Point(bbox[1], bbox[2]), cv::Point(bbox[5], bbox[6]), cv::Scalar(0, 255, 0), 2);
    // Display class label
    cv::putText(image, className, cv::Point(bbox[1], bbox[2] - 10), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
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

    const char* get_access_token_helper(){
        if (final_access_token == ""){
            const char* username = "ananda.hexacon.2@invigilo.sg";
            const char* password = "1y2u3i4o5P!()";
            final_access_token = get_auth_bear(strdup(username), strdup(password));
        }
        return final_access_token;
    }

    void upload_incident() {
        std::string server_access_token = "9e707232796144bc5966b68f10b11abc";
        int ai_mapping_id = 3561;
        std::string incident_uuid = "c9761efc-b170-4227-8127-3f214eba2021";
        std::string url = "http://stag.invigilo.ai/api/v1/incidents";

        std::string json_data = "{\"type\": [1, 2, 3], \"meta\": {}, \"extra\": {}, \"count\": 0, \"frame\": \"string\", \"unprocessed_frame\": \"string\", \"video\": \"string\", \"people\": 0, \"objects\": 0, \"is_case\": false, \"uuid\": \"" + incident_uuid + "\", \"ai_mapping_id\": " + std::to_string(ai_mapping_id) + "}";

        std::thread requestThread(perform_curl_request, url, json_data, server_access_token);
        std::cout << "Spawned Thread for curl" << std::endl;
        //requestThread.join();
    }

    // void upload_image_to_s3(){
    //     std::string access_key_id = "AKIA6HF4GGC52SEVW34C";
    //     std::string secret_access_key = "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";
    //     std::string bucket_name = "dataset-invigilo";
    //     std::string file_path = "/download_frames/OSD_frame_8_0.jpg";
    //     std::string s3_key = "/deepstream_incidents/OSD_frame_8_0.jpg";

    //     std::thread upload_thread(upload_image_to_S3, access_key_id, secret_access_key, bucket_name, file_path, s3_key);
    //     std::cout << "Spawned Thread for s3" << std::endl;
    //     upload_thread.join();

    //     return;
    // }

    void process_annotation_helper() {
        cv::Mat image = cv::imread("/opt/nvidia/deepstream/deepstream-6.2/DeepStream-Yolo/download_frames/OSD_frame_8_0.jpg");
        if (image.empty()) {
            std::cerr << "Error: Could not read the image." << std::endl;
            return;
        }

        std::ifstream annotationFile("/opt/nvidia/deepstream/deepstream-6.2/DeepStream-Yolo/detections/00_000_000008.txt");
        if (!annotationFile.is_open()) {
            std::cerr << "Error: Could not open the annotation file." << std::endl;
            return;
        }

        std::vector<std::thread> threads;
        std::string line;
        while (std::getline(annotationFile, line)) {
            // Create a thread for each annotation
            threads.emplace_back([line, &image]() {
                std::lock_guard<std::mutex> lock(mtx); // Lock to protect shared data
                process_annotation(line, image);
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        cv::imwrite("/opt/nvidia/deepstream/deepstream-6.2/DeepStream-Yolo/download_frames/annotated_image.jpg", image);
        cv::waitKey(0);

        std::cout<<"saved annotated image"<<std::endl;

        return;
    }

}
