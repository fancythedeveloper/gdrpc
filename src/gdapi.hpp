#pragma once
#ifndef GDAPI_H
#define GDAPI_H
#include "gjgamelevel.hpp"
#include "pch.h"

#include <algorithm>
#include <exception>
#include <httplib.h>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class difficulty {
  na,
  easy,
  normal,
  hard,
  harder,
  insane,
}; // note that no demon difficulty exists

// thinking about this a bit later
// probably shouldn't have used structs lol

struct GDlevel {
  int levelID = -1;
  std::string name;
  std::string author = "-";
  int authorID = -1;
  int stars = 0;
  difficulty difficulty = difficulty::na;
  bool isAuto = false;
  bool isDemon = false;
}; // this is a really barebones struct btw

struct GDuser {
  int ID = -1;
  int accID = -1;
  std::string name;
  int rank = -1;
};

struct GDUrls {
  std::string get_user_info = "incl/profiles/getGJUserInfo.php";
  std::string get_users = "incl/profiles/getGJUsers.php";
  std::string get_scores = "getGJScores19.php";
};

difficulty getDiffValue(int diff);
std::string getDifficultyName(GDlevel &level);

typedef std::multimap<std::string, std::string> Params;
typedef std::unordered_map<int, std::string> Robtop_Map;

class GD_Client {
private:
  std::string host;
  std::string prefix;

  const int game_version;
  const std::string secret;

  GDUrls urls;

  std::shared_ptr<httplib::Client> client;

  // makes an internet post request to boomlings.com
  std::string post_request(std::string, Params &);

public:
  GD_Client(std::string host = "http://boomlings.com",
            std::string prefix = "/database/");

  bool get_user_info(int &accID, GDuser &user);
  bool get_player_info(int &playerID, GDuser &user);

  bool get_user_rank(GDuser &user);

  void set_urls(GDUrls);
};

Robtop_Map to_robtop(std::string &, char delimiter = ':');

// splits a string by substring, much like in other languages
std::vector<std::string> explode(std::string &string, char separator);

bool parseGJGameLevel(GJGameLevel *in_memory, GDlevel &level);

#endif // !GDAPI_H