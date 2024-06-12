#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <tuple>
#include <json-c/json.h>
#include <pbnjson.hpp>

// tomlSectionObject mapping { config param -> config values }
typedef std::unordered_map<std::string, std::string> tomlSectionObject;

// tomlObject mapping { section -> tomlSectionObject }
typedef std::unordered_map<std::string, tomlSectionObject> tomlObject;

std::string trim_string(const std::string &str, size_t first, size_t last);
std::string trim_string(const std::string &str);

tomlObject readTomlFile(std::string filePath);

bool writeTomlSection(const std::string &filePath, const std::string &sectionName, const tomlSectionObject &obj);

void writeTomlFile(const std::string &filePath, const tomlObject &obj);

// void displayTomlFile(tomlObject & obj, std::string outFile = "");

std::tuple<bool, tomlObject> jsonStringToTomlObject(const char *jsonStr);

std::string tomlObjectToJsonString(const tomlObject &obj, const std::string &initialIndentation);

bool fileExists(const char * filePath);

pbnjson::JValue stringToJValue(const char *rawData);