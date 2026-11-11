#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Navigation.h"

namespace hb {

using json = nlohmann::json;

// Flat JSON settings file. Unknown keys are dropped, wrong types fall back to defaults.
class Settings
{
public:
    explicit Settings(std::filesystem::path file) : file_(std::move(file)) {}

    static json Defaults();

    void Load();
    bool Save() const;

    const json& All() const noexcept { return data_; }
    bool Has(const std::string& key) const { return data_.contains(key); }

    template <class T>
    T Get(const std::string& key) const
    {
        try { return data_.at(key).get<T>(); }
        catch (...) { return Defaults().at(key).get<T>(); }
    }

    // Returns false when the key is unknown, the type does not match the default or the value is not allowed
    // (unknown theme name, malformed colour, out-of-range number, unsafe pinned URL, bad search template ...).
    bool Set(const std::string& key, const json& value);

    // Value rules shared by Set() and Load(); a file edited by hand can therefore not smuggle in bad values.
    static bool IsValid(const std::string& key, const json& value);

    // Custom search engines stored under "customSearchEngines" -> engines usable by SetCustomSearchEngines().
    static std::vector<SearchEngine> CustomEngines(const json& list);

    std::function<void(const std::string& key)> onChanged;

private:
    std::filesystem::path file_;
    json data_ = Defaults();
};

}  // namespace hb
