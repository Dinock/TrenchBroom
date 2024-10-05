/*
 Copyright (C) 2010-2017 Kristian Duske

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

#include "ShaderConfig.h"

#include <string>
#include <vector>

namespace TrenchBroom::Renderer
{

ShaderConfig::ShaderConfig(
  std::string name,
  std::vector<std::string> vertexShaders,
  std::vector<std::string> fragmentShaders)
  : m_name{std::move(name)}
  , m_vertexShaders{std::move(vertexShaders)}
  , m_fragmentShaders{std::move(fragmentShaders)}
{
}

const std::string& ShaderConfig::name() const
{
  return m_name;
}

const std::vector<std::string>& ShaderConfig::vertexShaders() const
{
  return m_vertexShaders;
}

const std::vector<std::string>& ShaderConfig::fragmentShaders() const
{
  return m_fragmentShaders;
}

} // namespace TrenchBroom::Renderer
