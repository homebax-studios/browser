#pragma once
#include <filesystem>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

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

    // Returns false when the key is unknown or the type does not match the default.
    bool Set(const std::string& key, const json& value);

    std::function<void(const std::string& key)> onChanged;

private:
    std::filesystem::path file_;
    json data_ = Defaults();
};

}  // namespace hb
