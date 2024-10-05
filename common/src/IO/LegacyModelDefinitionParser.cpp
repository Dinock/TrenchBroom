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

#include "LegacyModelDefinitionParser.h"

#include "Assets/ModelDefinition.h"
#include "EL/Expression.h"
#include "EL/Value.h"
#include "IO/ParserStatus.h"

#include "kdl/string_compare.h"

#include <fmt/format.h>

#include <algorithm>
#include <string>

namespace TrenchBroom::IO
{

LegacyModelDefinitionTokenizer::LegacyModelDefinitionTokenizer(
  const std::string_view str, const size_t line, const size_t column)
  : Tokenizer{str, "", 0, line, column}
{
}

const std::string LegacyModelDefinitionTokenizer::WordDelims = " \t\n\r()[]{};,=";

LegacyModelDefinitionTokenizer::Token LegacyModelDefinitionTokenizer::emitToken()
{
  while (!eof())
  {
    const auto startLine = line();
    const auto startColumn = column();
    const auto startLocation = location();
    const auto* c = curPos();
    switch (*c)
    {
    case '=':
      advance();
      return Token{MdlToken::Equality, c, c + 1, offset(c), startLine, startColumn};
    case ')':
      advance();
      return Token{MdlToken::CParenthesis, c, c + 1, offset(c), startLine, startColumn};
    case ',':
      advance();
      return Token{MdlToken::Comma, c, c + 1, offset(c), startLine, startColumn};
    case ' ':
    case '\t':
    case '\n':
    case '\r':
      advance();
      break;
    case '"':
      advance();
      c = curPos();
      return Token{
        MdlToken::String, c, readQuotedString(), offset(c), startLine, startColumn};
    default: {
      if (const auto* e = readInteger(WordDelims))
      {
        return Token{MdlToken::Integer, c, e, offset(c), startLine, startColumn};
      }
      if (const auto* e = readUntil(WordDelims))
      {
        return Token{MdlToken::Word, c, e, offset(c), startLine, startColumn};
      }
      throw ParserException{startLocation, fmt::format("Unexpected character: {}", *c)};
    }
    }
  }
  return Token{MdlToken::Eof, nullptr, nullptr, length(), line(), column()};
}

LegacyModelDefinitionParser::LegacyModelDefinitionParser(
  const std::string_view str, const size_t line, const size_t column)
  : m_tokenizer{str, line, column}
{
}

TokenizerState LegacyModelDefinitionParser::tokenizerState() const
{
  return m_tokenizer.snapshot();
}

EL::ExpressionNode LegacyModelDefinitionParser::parse(ParserStatus& status)
{
  return parseModelDefinition(status);
}

EL::ExpressionNode LegacyModelDefinitionParser::parseModelDefinition(ParserStatus& status)
{
  auto token = m_tokenizer.peekToken();
  const auto startLocation = token.location();

  expect(status, MdlToken::String | MdlToken::Word | MdlToken::CParenthesis, token);
  if (token.hasType(MdlToken::CParenthesis))
  {
    return EL::ExpressionNode{
      EL::LiteralExpression{EL::Value::Undefined}, token.location()};
  }

  auto modelExpressions = std::vector<EL::ExpressionNode>{};
  do
  {
    expect(status, MdlToken::String | MdlToken::Word, token = m_tokenizer.peekToken());
    if (token.hasType(MdlToken::String))
    {
      modelExpressions.push_back(parseStaticModelDefinition(status));
    }
    else
    {
      modelExpressions.push_back(parseDynamicModelDefinition(status));
    }

    expect(
      status, MdlToken::Comma | MdlToken::CParenthesis, token = m_tokenizer.peekToken());
    if (token.hasType(MdlToken::Comma))
    {
      m_tokenizer.nextToken();
    }
  } while (token.hasType(MdlToken::Comma));

  // The legacy model expressions are evaluated back to front.
  std::reverse(std::begin(modelExpressions), std::end(modelExpressions));
  return EL::ExpressionNode{
    EL::SwitchExpression{std::move(modelExpressions)}, startLocation};
}

EL::ExpressionNode LegacyModelDefinitionParser::parseStaticModelDefinition(
  ParserStatus& status)
{
  auto token = m_tokenizer.nextToken();
  expect(status, MdlToken::String, token);
  const auto startLocation = token.location();

  auto map = EL::MapType{};
  map[Assets::ModelSpecificationKeys::Path] = EL::Value(token.data());

  auto indices = std::vector<size_t>{};

  expect(
    status,
    MdlToken::Integer | MdlToken::Word | MdlToken::Comma | MdlToken::CParenthesis,
    token = m_tokenizer.peekToken());
  if (token.hasType(MdlToken::Integer))
  {
    token = m_tokenizer.nextToken();
    indices.push_back(token.toInteger<size_t>());
    expect(
      status,
      MdlToken::Integer | MdlToken::Word | MdlToken::Comma | MdlToken::CParenthesis,
      token = m_tokenizer.peekToken());
    if (token.hasType(MdlToken::Integer))
    {
      token = m_tokenizer.nextToken();
      indices.push_back(token.toInteger<size_t>());
      expect(
        status,
        MdlToken::Word | MdlToken::Comma | MdlToken::CParenthesis,
        token = m_tokenizer.peekToken());
    }
  }

  if (!indices.empty())
  {
    map[Assets::ModelSpecificationKeys::Skin] = EL::Value(indices[0]);
    if (indices.size() > 1)
    {
      map[Assets::ModelSpecificationKeys::Frame] = EL::Value(indices[1]);
    }
  }

  auto modelExpression =
    EL::ExpressionNode{EL::LiteralExpression{EL::Value{std::move(map)}}, startLocation};

  if (token.hasType(MdlToken::Word))
  {
    token = m_tokenizer.nextToken();

    auto attributeKey = token.data();
    const auto location = token.location();
    auto keyExpression =
      EL::ExpressionNode{EL::VariableExpression{std::move(attributeKey)}, location};

    expect(status, MdlToken::Equality, token = m_tokenizer.nextToken());

    expect(status, MdlToken::String | MdlToken::Integer, token = m_tokenizer.nextToken());
    if (token.hasType(MdlToken::String))
    {
      auto attributeValue = token.data();
      auto valueExpression = EL::ExpressionNode{
        EL::LiteralExpression{EL::Value{std::move(attributeValue)}}, token.location()};
      auto premiseExpression = EL::ExpressionNode{
        EL::BinaryExpression{
          EL::BinaryOperation::Equal,
          std::move(keyExpression),
          std::move(valueExpression)},
        location};

      return EL::ExpressionNode{
        EL::BinaryExpression{
          EL::BinaryOperation::Case,
          std::move(premiseExpression),
          std::move(modelExpression)},
        startLocation};
    }
    const auto flagValue = token.toInteger<int>();
    auto valueExpression =
      EL::ExpressionNode{EL::LiteralExpression{EL::Value{flagValue}}, token.location()};
    auto premiseExpression = EL::ExpressionNode{
      EL::BinaryExpression{
        EL::BinaryOperation::Equal, std::move(keyExpression), std::move(valueExpression)},
      location};

    return EL::ExpressionNode{
      EL::BinaryExpression{
        EL::BinaryOperation::Case,
        std::move(premiseExpression),
        std::move(modelExpression)},
      startLocation};
  }

  return modelExpression;
}

EL::ExpressionNode LegacyModelDefinitionParser::parseDynamicModelDefinition(
  ParserStatus& status)
{
  auto token = m_tokenizer.peekToken();
  const auto location = token.location();

  auto map = std::map<std::string, EL::ExpressionNode>{
    {Assets::ModelSpecificationKeys::Path, parseNamedValue(status, "pathKey")},
  };

  expect(
    status, MdlToken::Word | MdlToken::CParenthesis, token = m_tokenizer.peekToken());

  if (!token.hasType(MdlToken::CParenthesis))
  {
    do
    {
      if (kdl::ci::str_is_equal("skinKey", token.data()))
      {
        map.emplace(
          Assets::ModelSpecificationKeys::Skin, parseNamedValue(status, "skinKey"));
      }
      else if (kdl::ci::str_is_equal("frameKey", token.data()))
      {
        map.emplace(
          Assets::ModelSpecificationKeys::Frame, parseNamedValue(status, "frameKey"));
      }
      else
      {
        const auto msg =
          fmt::format("Expected 'skinKey' or 'frameKey', but found '{}'", token.data());
        status.error(token.location(), msg);
        throw ParserException{token.location(), msg};
      }
    } while (
      expect(
        status, MdlToken::Word | MdlToken::CParenthesis, token = m_tokenizer.peekToken())
        .hasType(MdlToken::Word));
  }

  return EL::ExpressionNode{EL::MapExpression{std::move(map)}, location};
}

EL::ExpressionNode LegacyModelDefinitionParser::parseNamedValue(
  ParserStatus& status, const std::string& name)
{
  auto token = Token{};
  expect(status, MdlToken::Word, token = m_tokenizer.nextToken());

  const auto location = token.location();
  if (!kdl::ci::str_is_equal(name, token.data()))
  {
    throw ParserException{
      location, fmt::format("Expected '{}', but got '{}'", name, token.data())};
  }

  expect(status, MdlToken::Equality, token = m_tokenizer.nextToken());
  expect(status, MdlToken::String, token = m_tokenizer.nextToken());

  return EL::ExpressionNode{EL::VariableExpression{token.data()}, location};
}

LegacyModelDefinitionParser::TokenNameMap LegacyModelDefinitionParser::tokenNames() const
{
  using namespace MdlToken;

  return {
    {Integer, "integer"},
    {String, "quoted string"},
    {Word, "word"},
    {Comma, "','"},
    {Equality, "'='"},
    {CParenthesis, "')'"},
    {Eof, "end of file"},
  };
}
} // namespace TrenchBroom::IO
