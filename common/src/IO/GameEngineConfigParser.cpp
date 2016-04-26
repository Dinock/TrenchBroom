/*
 Copyright (C) 2010-2016 Kristian Duske
 
 This file is part of TrenchBroom.
 
 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "GameEngineConfigParser.h"

#include "CollectionUtils.h"
#include "Exceptions.h"

namespace TrenchBroom {
    namespace IO {
        GameEngineConfigParser::GameEngineConfigParser(const char* begin, const char* end, const Path& path) :
        ConfigParserBase(begin, end, path) {}
        
        GameEngineConfigParser::GameEngineConfigParser(const String& str, const Path& path) :
        ConfigParserBase(str, path) {}
        
        Model::GameEngineConfig GameEngineConfigParser::parse() {
            ConfigPtr root = parseConfigFile();
            if (root.get() == NULL)
                throw ParserException("Empty game engine config");
            
            expectEntry(ConfigEntry::Type_Table, *root);
            const ConfigTable& rootTable = *root;
            
            expectTableEntries(rootTable,
                               StringUtils::makeSet(2, "version", "profiles"),
                               StringSet());
            
            expectTableEntry("version", ConfigEntry::Type_Value, rootTable);
            const String version = rootTable["version"];
            
            expectTableEntry("profiles", ConfigEntry::Type_List, rootTable);
            const Model::GameEngineProfile::List profiles = parseProfiles(rootTable["profiles"]);
            
            return Model::GameEngineConfig(profiles);
        }

        Model::GameEngineProfile::List GameEngineConfigParser::parseProfiles(const ConfigList& list) const {
            Model::GameEngineProfile::List result;
            
            try {
                for (size_t i = 0; i < list.count(); ++i) {
                    expectListEntry(i, ConfigEntry::Type_Table, list);
                    result.push_back(parseProfile(list[i]));
                }
                return result;
            } catch (...) {
                VectorUtils::clearAndDelete(result);
                throw;
            }
        }
        
        Model::GameEngineProfile* GameEngineConfigParser::parseProfile(const ConfigTable& table) const {
            expectTableEntries(table,
                               StringUtils::makeSet(2, "name", "path"),
                               StringSet());
            
            expectTableEntry("name", ConfigEntry::Type_Value, table);
            const String name = table["name"];
            
            expectTableEntry("path", ConfigEntry::Type_Value, table);
            const String path = table["path"];
            
            return new Model::GameEngineProfile(name, path);
        }
    }
}
