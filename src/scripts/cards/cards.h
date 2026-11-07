#pragma once

#include <functional>
#include <random>
#include <string>
#include <vector>

struct CardInfo {
  std::string code;  // "G101", "E203", "N301", "FE01"
  std::string name;
  std::string description;
  char category = 'N';  // 'G' good, 'E' evil, 'N' neutral, 'F' field
  std::string category_name;
  int hierarchy = 0;  // 1 (rarest) to 4; 0 for field cards
  std::string hierarchy_name;
  int attack = 0;
  int defense = 0;
  int odds = 1;  // deck draw weight; higher = more common
  bool combinable = false;
  std::string image_url;
  // Field cards only: favored category ("good"/"evil"), buff and nerf.
  std::string favored;
  int buff = 0;
  int nerf = 0;

  bool IsField() const { return category == 'F'; }
  std::string CategoryLabel() const;
  const char* FrameSprite() const;
};

namespace cards {

std::vector<CardInfo> LoadAll(const std::string& base);

const CardInfo* Find(const std::vector<CardInfo>& all, const std::string& code);

// Weighted draw; copy limits from rules::Get(); weight 0 never enters.
using Weight = std::function<int(const CardInfo&)>;
std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, int size, std::mt19937& rng,
                                   const Weight& weight = Weight());

}
