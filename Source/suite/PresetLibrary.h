#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <optional>
#include <vector>

// Factory names are read-only. User presets are files in directory.
// Favorites are names in favorites.txt beside those files.
class PresetLibrary {
public:
    struct Entry {
        juce::String name;
        bool factory = false;
        bool favorite = false;
        int factoryIndex = -1;
    };
    PresetLibrary(juce::File directory, std::vector<juce::String> factoryNames);
    static juce::File userDirectory(const juce::String& pluginName);
    const juce::File& folder() const noexcept { return directory; }
    bool isFactoryName(const juce::String& name) const;
    std::vector<Entry> entries(const juce::String& search) const;
    bool save(const juce::String& name, const juce::ValueTree& state, juce::String& error);
    bool rename(const juce::String& from, const juce::String& to, juce::String& error);
    bool remove(const juce::String& name, juce::String& error);
    bool setFavorite(const juce::String& name, bool favorite);
    bool isFavorite(const juce::String& name) const;
    std::optional<juce::ValueTree> loadUser(const juce::String& name) const;
    static juce::String sanitise(juce::String name);
private:
    std::vector<juce::String> readFavorites() const;
    void writeFavorites(const std::vector<juce::String>& names) const;
    juce::File fileFor(const juce::String& name) const;
    juce::File directory;
    std::vector<juce::String> factory;
};
