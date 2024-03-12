#include "tomlParser.h"
#include <fstream>
#include <iostream> // to be removed
#include <sstream>

std::string trim_string(const std::string &str, int first, int last)
{
    int n = (int)str.size();
    if ((first >= n) || (last < 0))
        return "";
    while ((first < n) && (str[first] == ' '))
        first++;
    while ((last >= 0) && (str[last] == ' '))
        last--;
    if (first > last)
        return "";
    return str.substr(first, last - first + 1);
}

std::string trim_string(const std::string &str)
{
    int n = (int)str.size();
    int first = 0;
    int last = n - 1;

    if ((first >= n) || (last < 0))
        return "";
    while ((first < n) && (str[first] == ' '))
        first++;
    while ((last >= 0) && (str[last] == ' '))
        last--;
    if (first > last)
        return "";
    return str.substr(first, last - first + 1);
}

void _removeAllSpaces(std::string &str)
{
    size_t pos = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] != ' ')
        {
            str[pos++] = str[i];
        }
    }
    str.erase(str.begin() + pos, str.end());
}

std::string removeAllSpaces(std::string str)
{
    size_t pos = 0;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] != ' ')
        {
            str[pos++] = str[i];
        }
    }
    str.erase(str.begin() + pos, str.end());
    return str;
}

std::vector<std::string> parseConfigValue(std::string &str)
{
    _removeAllSpaces(str);

    if (str[0] != '[')
        return {str};
    std::vector<std::string> ret;
    size_t prev = 1;
    do
    {
        size_t pos = str.find(',', prev);
        if (pos == std::string::npos)
            break;
        ret.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    } while (true);
    ret.push_back(str.substr(prev, str.size() - prev - 1));
    return ret;
}

// just a toml reader version for use in sdkagent service, not for general toml parsing
tomlObject readTomlFile(std::string filePath)
{
    tomlObject ret;

    std::ifstream fp(filePath);
    if (!fp || (fp.fail()))
    {
        fp.close();
        return ret;
    }

    std::string line;
    std::string section;

    while (std::getline(fp, line))
    {
        auto pos = line.find_first_not_of(' ');

        // ignore empty and comment line
        if ((pos >= line.size()) || (line[pos] == '#'))
            continue;

        // get section name
        if (line[pos] == '[')
        {
            section = "";
            auto sb = line.find_first_not_of("[ ", pos + 1);
            auto se = line.find_last_not_of("] ");
            section = line.substr(sb, se - sb + 1);
            ret[section] = {};
            continue;
        }

        auto equalPos = line.find('=');
        if (equalPos == std::string::npos)
        {
            ret = {};
            break;
        }
        else
        {
            std::string configParam = trim_string(line, 0, (int)equalPos - 1);
            ret[section][configParam] = removeAllSpaces(line.substr(equalPos + 1, line.size() - equalPos - 1));
        }
    }
    fp.close();

    return ret;
}

bool writeTomlSection(const std::string &filePath, const std::string &sectionName, const tomlSectionObject &obj)
{
    std::stringstream ss;
    ss << '[' << sectionName << "]\n";
    for (const auto &sectionData : obj)
    {
        ss << sectionData.first << "=" << sectionData.second << '\n';
    }

    std::ofstream fp(filePath);
    fp << ss.rdbuf();
    fp.close();

    return true;
}

void writeTomlFile(const std::string &filePath, const tomlObject &obj)
{
    std::stringstream ss;
    std::string spc = "  ";
    for (auto &it : obj)
    {
        ss << '[' << it.first << "]\n";
        for (auto &configParam : it.second)
        {
            ss << spc << configParam.first << '=' << configParam.second << '\n';
        }
    }

    std::ofstream fp(filePath);
    fp << ss.rdbuf();
    fp.close();
}

std::string fixDoubleForwardSlash(std::string str)
{
    std::string token; //    92 \       47 /
    token.push_back(92);
    token.push_back(47);
    token.push_back(92);
    token.push_back(47);

    size_t pos = str.find(token);
    if (pos != std::string::npos)
    {
        str.replace(pos, token.size(), "//");
        return str;
    }

    return str;
}

std::string toStringArr(json_object *jValues)
{
    std::string cvalue = "[";
    for (size_t i = 0; i < json_object_array_length(jValues); i++)
    {
        json_object *val = json_object_array_get_idx(jValues, i);
        cvalue += fixDoubleForwardSlash(std::string(json_object_to_json_string(val)));
        cvalue += ",";
    }
    cvalue.back() = ']';
    return cvalue;
}

std::tuple<bool, tomlObject> jsonStringToTomlObject(const char *jsonStr)
{
    tomlObject ret;
    json_object *obj = json_tokener_parse(jsonStr);
    if (!obj)
    {
        return std::make_tuple(false, ret);
    }

    json_object_object_foreach(obj, section, sectionValue)
    {
        ret[section] = {};
        json_object_object_foreach(sectionValue, configParam, configValue)
        {
            switch (json_object_get_type(configValue))
            {
            case json_type::json_type_array:
                ret[section][configParam] = toStringArr(configValue);
                break;

            default:
                ret[section][configParam] = fixDoubleForwardSlash(std::string(json_object_to_json_string(configValue)));
                break;
            }
        }
    }

    return std::make_tuple(true, ret);
}

std::string tomlObjectToJsonString(const tomlObject &obj, const std::string &initialIndentation)
{
    std::string ret = "";
    const std::string tab = "    ";
    std::string indent = initialIndentation;

    ret = indent + "{\n";
    indent += tab;
    for (auto &section : obj)
    {
        ret += (indent + "\"" + section.first + "\": {\n");
        indent += tab;
        for (auto &config : section.second)
        {
            if (section.second.empty())
                continue;
            ret += indent + "\"" + config.first + "\": " + config.second + ",\n";
        }

        // remove last ,
        if (!section.second.empty())
        {
            ret[ret.size() - 2] = ' ';
        }

        // adjust the indent
        indent = indent.substr(0, indent.size() - tab.size());

        // add closing bracket here
        ret += (indent + "},\n");
    }
    // remove last ,
    if (!obj.empty())
    {
        ret[ret.size() - 2] = ' ';
    }
    indent = indent.substr(0, indent.size() - tab.size());
    ret += (indent + "}");

    return ret;
}

// void displayTomlFile(tomlObject & obj, std::string outFile)
// {
//     if (outFile.empty()) {
//         for (auto & section : obj) {
//             std::cout << '[' << section.first << ']' << std::endl;
//             for (auto & config : section.second) {
//                 std::cout << "    " << config.first << ": " << config.second << '\n';
//             }
//         }
//     }
//     else {
//         std::ofstream fp(outFile, fstream::app|fstream::out);
//         for (auto & section : obj) {
//             fp << '[' << section.first << "]\n";
//             for (auto & config : section.second) {
//                 fp << "    " << config.first << ": " << config.second << '\n';
//             }
//         }
//         fp.close();
//     }
// }

pbnjson::JValue stringToJValue(const char *rawData)
{
    pbnjson::JInput input(rawData);
    pbnjson::JSchema schema = pbnjson::JSchemaFragment("{}");
    pbnjson::JDomParser parser;
    if (!parser.parse(input, schema))
    {
        return pbnjson::JValue();
    }
    return parser.getDom();
}
