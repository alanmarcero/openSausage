#include "PresetLibrary.h"
#include <algorithm>

PresetLibrary::PresetLibrary(juce::File directoryIn, std::vector<juce::String> factoryNames)
    : directory(std::move(directoryIn)), factory(std::move(factoryNames)) {}

juce::File PresetLibrary::userDirectory(const juce::String& pluginName) {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Alan Marcero").getChildFile(pluginName).getChildFile("Presets");
}
juce::String PresetLibrary::sanitise(juce::String name) {
    name = name.trim();
    if (name.endsWithIgnoreCase(".preset")) name = name.dropLastCharacters(7).trim();
    juce::String clean;
    for (const auto character : name) {
        if (character < 32 || juce::String("\\/:*?\"<>|").containsChar(character)) continue;
        clean += character;
    }
    clean = clean.trim();
    if (clean.length() > 80) clean = clean.substring(0, 80).trim();
    if (clean == "." || clean == "..") return {};
    return clean;
}
bool PresetLibrary::isFactoryName(const juce::String& name) const {
    const auto trimmed = name.trim();
    return std::ranges::any_of(factory, [&](const juce::String& factoryName) { return factoryName.equalsIgnoreCase(trimmed); });
}
juce::File PresetLibrary::fileFor(const juce::String& name) const {
    return directory.getChildFile(sanitise(name) + ".preset");
}
std::vector<juce::String> PresetLibrary::readFavorites() const {
    std::vector<juce::String> names;
    const auto file = directory.getChildFile("favorites.txt");
    if (!file.existsAsFile()) return names;
    for (const auto& line : juce::StringArray::fromLines(file.loadFileAsString()))
        if (const auto trimmed = line.trim(); trimmed.isNotEmpty()) names.push_back(trimmed);
    return names;
}
void PresetLibrary::writeFavorites(const std::vector<juce::String>& names) const {
    directory.createDirectory();
    juce::StringArray lines;
    for (const auto& name : names) lines.add(name);
    directory.getChildFile("favorites.txt").replaceWithText(lines.joinIntoString("\n") + (lines.isEmpty() ? "" : "\n"));
}
bool PresetLibrary::isFavorite(const juce::String& name) const {
    const auto trimmed = name.trim();
    return std::ranges::any_of(readFavorites(), [&](const juce::String& favorite) { return favorite.equalsIgnoreCase(trimmed); });
}
bool PresetLibrary::setFavorite(const juce::String& name, bool favorite) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty() || trimmed.containsAnyOf("\r\n")) return false;
    auto names = readFavorites();
    std::erase_if(names, [&](const juce::String& entry) { return entry.equalsIgnoreCase(trimmed); });
    if (favorite) names.push_back(trimmed);
    writeFavorites(names);
    return true;
}
std::vector<PresetLibrary::Entry> PresetLibrary::entries(const juce::String& search) const {
    const auto query = search.trim();
    const auto matches = [&](const juce::String& name) { return query.isEmpty() || name.containsIgnoreCase(query); };
    const auto favorites = readFavorites();
    const auto favorite = [&](const juce::String& name) {
        return std::ranges::any_of(favorites, [&](const juce::String& entry) { return entry.equalsIgnoreCase(name); });
    };
    std::vector<Entry> factoryEntries, userEntries;
    for (int index = 0; index < static_cast<int>(factory.size()); ++index) {
        const auto& name = factory[static_cast<std::size_t>(index)];
        if (matches(name)) factoryEntries.push_back({name, true, favorite(name), index});
    }
    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, false, "*.preset");
    for (const auto& file : files) {
        const auto name = file.getFileNameWithoutExtension();
        if (isFactoryName(name) || !matches(name)) continue;
        userEntries.push_back({name, false, favorite(name), -1});
    }
    std::ranges::sort(userEntries, [](const Entry& left, const Entry& right) {
        return left.name.compareIgnoreCase(right.name) < 0;
    });
    std::vector<Entry> result;
    for (const auto& entry : factoryEntries) if (entry.favorite) result.push_back(entry);
    for (const auto& entry : userEntries) if (entry.favorite) result.push_back(entry);
    for (const auto& entry : factoryEntries) if (!entry.favorite) result.push_back(entry);
    for (const auto& entry : userEntries) if (!entry.favorite) result.push_back(entry);
    return result;
}
bool PresetLibrary::save(const juce::String& name, const juce::ValueTree& state, juce::String& error) {
    const auto clean = sanitise(name);
    if (clean.isEmpty()) { error = "Type a preset name."; return false; }
    if (isFactoryName(name) || isFactoryName(clean)) { error = "Factory presets stay read-only."; return false; }
    if (!directory.createDirectory().wasOk()) { error = "Could not create the preset folder."; return false; }
    const auto xml = state.createXml();
    if (xml == nullptr || !fileFor(clean).replaceWithText(xml->toString())) { error = "Could not save the preset."; return false; }
    error.clear();
    return true;
}
std::optional<juce::ValueTree> PresetLibrary::loadUser(const juce::String& name) const {
    if (isFactoryName(name)) return std::nullopt;
    const auto file = fileFor(name);
    if (!file.existsAsFile()) return std::nullopt;
    if (const auto xml = juce::parseXML(file)) return juce::ValueTree::fromXml(*xml);
    return std::nullopt;
}
bool PresetLibrary::rename(const juce::String& from, const juce::String& to, juce::String& error) {
    const auto source = sanitise(from);
    const auto dest = sanitise(to);
    if (source.isEmpty() || isFactoryName(from) || isFactoryName(source)) { error = "Factory presets stay read-only."; return false; }
    if (dest.isEmpty()) { error = "Type a new name."; return false; }
    if (isFactoryName(to) || isFactoryName(dest)) { error = "Factory presets stay read-only."; return false; }
    const auto sourceFile = fileFor(source);
    if (!sourceFile.existsAsFile()) { error = "Preset not found."; return false; }
    if (source.equalsIgnoreCase(dest)) { error.clear(); return true; }
    const auto destFile = fileFor(dest);
    if (destFile.existsAsFile()) { error = "A preset with that name already exists."; return false; }
    if (!sourceFile.moveFileTo(destFile)) { error = "Could not rename the preset."; return false; }
    if (isFavorite(source)) { setFavorite(source, false); setFavorite(dest, true); }
    error.clear();
    return true;
}
bool PresetLibrary::remove(const juce::String& name, juce::String& error) {
    if (isFactoryName(name)) { error = "Factory presets stay read-only."; return false; }
    const auto clean = sanitise(name);
    const auto file = fileFor(clean);
    if (clean.isEmpty() || !file.existsAsFile()) { error = "Preset not found."; return false; }
    if (!file.deleteFile()) { error = "Could not delete the preset."; return false; }
    setFavorite(clean, false);
    error.clear();
    return true;
}
