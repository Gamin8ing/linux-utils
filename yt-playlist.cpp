#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <curl/curl.h>
#include "json.hpp" // Download from https://github.com/nlohmann/json

using namespace std;
using json = nlohmann::json;

std::string PLAYLIST_ID = "PLAYLIST_ID";

// Function to handle HTTP response
size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                     string *output) {
  size_t totalSize = size * nmemb;
  output->append((char *)contents, totalSize);
  return totalSize;
}

// Function to make API request
string makeApiRequest(const string &url) {
  CURL *curl = curl_easy_init();
  string response;

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      cerr << "cURL Error: " << curl_easy_strerror(res) << endl;
      return "";
    }
  }
  return response;
}

// Function to extract video IDs from playlist
vector<string> getVideoIds() {
  vector<string> videoIds;
  string nextPageToken = "";

  do {
    string apiUrl = "https://www.googleapis.com/youtube/v3/"
                    "playlistItems?part=contentDetails"
                    "&maxResults=50&playlistId=" +
                    PLAYLIST_ID + "&key=" + API_KEY;
    if (!nextPageToken.empty())
      apiUrl += "&pageToken=" + nextPageToken;

    string response = makeApiRequest(apiUrl);
    if (response.empty())
      break;

    json jsonData = json::parse(response);
    for (const auto &item : jsonData["items"]) {
      if (item.contains("contentDetails") &&
          item["contentDetails"].contains("videoId")) {
        videoIds.push_back(item["contentDetails"]["videoId"].get<string>());
      }
    }

    nextPageToken = jsonData.value("nextPageToken", "");

  } while (!nextPageToken.empty());

  return videoIds;
}

// Function to fetch video durations
vector<string> getVideoDurations(const vector<string> &videoIds, int start,
                                 int end) {
  vector<string> durations;

  for (size_t i = 0; i < videoIds.size(); i += 50) {
    string idBatch = "";
    for (size_t j = i; j < i + 50 && j < videoIds.size(); j++) {
      if (j + 1 < start || j + 1 > end)
        continue;
      if (!idBatch.empty())
        idBatch += ",";
      idBatch += videoIds[j];
    }

    string apiUrl =
        "https://www.googleapis.com/youtube/v3/videos?part=contentDetails"
        "&id=" +
        idBatch + "&key=" + API_KEY;

    string response = makeApiRequest(apiUrl);
    if (response.empty())
      continue;

    json jsonData = json::parse(response);
    for (const auto &item : jsonData["items"]) {
      if (item.contains("contentDetails") &&
          item["contentDetails"].contains("duration")) {
        durations.push_back(item["contentDetails"]["duration"].get<string>());
      }
    }
  }

  return durations;
}

// Function to convert ISO 8601 duration (PTxHxMxS) to total minutes
int parseDuration(const string &duration) {
  int hours = 0, minutes = 0, seconds = 0;
  size_t pos = 0;

  if ((pos = duration.find('H')) != string::npos) {
    hours = stoi(duration.substr(2, pos - 2));
  }

  if ((pos = duration.find('M')) != string::npos) {
    size_t start = duration.find('T') + 1;
    if (hours > 0)
      start = duration.find('H') + 1;
    minutes = stoi(duration.substr(start, pos - start));
  }

  if ((pos = duration.find('S')) != string::npos) {
    size_t start = duration.find('M') != string::npos ? duration.find('M') + 1
                                                      : duration.find('T') + 1;
    seconds = stoi(duration.substr(start, pos - start));
  }

  return (hours * 60) + minutes + (seconds >= 30 ? 1 : 0); // Round up seconds
}

// Main function
int main() {
  cout << "🔍 YouTube Playlist Analyzer 🔍" << endl;
  std::cout << "Enter the playlist ID: ";
  int start, end;
  std::cin >> PLAYLIST_ID;
  cout << "Fetching video durations..." << endl;

  vector<string> videoIds = getVideoIds();
  if (videoIds.empty()) {
    cerr << "No videos found in the playlist." << endl;
    return 1;
  }
  std::cout << videoIds.size() << " number of videos found!" << '\n';
  std::cout << "Enter the start,end of the playlist ";
  std::cin >> start >> end;

  vector<string> durations = getVideoDurations(videoIds, start, end);

  int totalMinutes = 0;
  cout << "\n🎥 Video Durations: " << endl;
  for (size_t i = 0; i < durations.size(); i++) {
    int minutes = parseDuration(durations[i]);
    totalMinutes += minutes;
    cout << "📌 Video " << (i + 1) << ": " << durations[i] << " (" << minutes
         << " min)" << endl;
  }

  int hours = totalMinutes / 60;
  int minutesLeft = totalMinutes % 60;
  cout << "-----------------------------------\n📊 Total Playlist Length: ";
  if (hours > 0) {
    cout << hours << " hours " << minutesLeft << " minutes";
  } else {
    cout << totalMinutes << " minutes";
  }
  cout << endl;

  cout << "📊 Average Video Length: ";
  int avgMinutes = totalMinutes / durations.size();
  if (avgMinutes >= 60) {
    cout << avgMinutes / 60 << " hours " << avgMinutes % 60 << " minutes";
  } else {
    cout << avgMinutes << " minutes";
  }
  cout << endl;

  return 0;
}
