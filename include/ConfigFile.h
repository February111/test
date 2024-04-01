#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <map>
#include <sstream>

class ConfigFile
{
public:
    ConfigFile() {}

    bool Load(const std::string &fileName)
    {
        std::ifstream ifs(fileName);
        if (ifs.good() == false)
        {
            std::cout << "ConfigFile::Load() : file not exist" << std::endl;
            return false;
        }

        std::string key;
        std::string value;
        std::string equ;
        //每次读一行
        std::string line;
        while (ifs.good())
        {
            std::getline(ifs, line);
            if (line.size() == 0)
            {
                break;
            }
            
            std::istringstream iss(line);
            iss >> key >> equ >> value;
            if (equ != "=" || value.size() == 0)
            {
                std::cout << "ConfigFile::Load(): config information error" << std::endl;
                return false;
            }
            _config[key] = value;
        }

        return true;
    }
    
    std::string Get(const std::string &key)
    {
        return _config[key];
    }

    ~ConfigFile() {}

private:
    std::map<std::string, std::string> _config;
};

