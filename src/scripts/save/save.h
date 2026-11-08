#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

// Player save (docs/menu.md): one JSON file in the user's home folder.
namespace save {

// One card earned: the starter deck or a battle reward.
struct Unlock {
  std::string code;
  std::string date;  // local time, "YYYY-MM-DD HH:MM:SS"
};

struct Data {
  std::string name;
  std::string language;
  int difficulty = 1;  // ai::Difficulty: 0 easy to 3 impossible
  bool tutorial = true;  // first battle still to play with the tutorial guide
  std::vector<Unlock> unlocked;  // history, oldest first
  std::vector<std::string> deck;  // battle deck: up to deckSize codes, repeats allowed

  // Unlocked copies grouped by card code.
  std::map<std::string, int> Available() const;
  int Available(const std::string& code) const;
  int InDeck(const std::string& code) const;
  void UnlockCard(const std::string& code);  // appends to the history with the current time
  // Adds or removes copies of `code` in the deck, up to the available ones and deckSize.
  void SetInDeck(const std::string& code, int copies);
};

// PONTIFICE_SAVE when set, otherwise {home}/pontifice.json.
std::string Path();

std::string ToJson(const Data& data);
// Deck entries beyond the available copies or deckSize are dropped.
bool FromJson(const std::string& text, Data& out);

// Missing file returns nullopt silently; an unreadable one logs and returns nullopt.
std::optional<Data> Load();
// Writes a temporary file and renames it over the save, so a crash keeps the old one.
bool Write(const Data& data);

}
