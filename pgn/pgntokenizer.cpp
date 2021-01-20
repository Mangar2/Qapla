/**
 * @license
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @author Volker Böhm
 * @copyright Copyright (c) 2021 Volker Böhm
 */

#include "pgntokenizer.h"

using namespace ChessPGN;

// -------------------------- CTor --------------------------------------------
PGNTokenizer::PGNTokenizer(void)
{
	static int aInit = false;
	if (!aInit)
	{
		aInit = true;
		int i;
		for (i = 0; i < 256; i++)
		{
			if (isSymbolContinuation(i))
				_charType[i] = CharType::SYMBOL_CONTINUATION;
			else _charType[i] = CharType::NOTHING;
		}
		_charType['\r'] = CharType::SPACE;
		_charType['\n'] = CharType::SPACE;
		_charType[' '] = CharType::SPACE;
	}
}

// -------------------------- SkipSpaces --------------------------------------
void PGNTokenizer::skipSpaces()
{
	for(; ; nextChar())
	{
		if (_pos >= _pgnString.length()) {
			break;
		}
		char ch = _pgnString[_pos];
		switch (ch)
		{
		case ' ': break;
		case '\n':
		case '\r':
			if (_pgnString[_pos + 1] == '%')
			{
				nextChar();
				findChar('\n', false);
			}
			break;
		default: return;
		}
	}
}

// -------------------------- FindChar ----------------------------------------
void PGNTokenizer::findChar(char ch, bool skipEOL)
{
	while ((_pgnString[_pos] != ch) && (_pgnString[_pos] != 0))
	{
		if (!skipEOL && (_pgnString[_pos] == '\n')) break;
		nextChar();
	}
}

// -------------------------- SkipAnnotation ----------------------------------
void PGNTokenizer::skipAnnotation()
{
	int aBracketAmount = 1; // First char is '('
	nextChar();				// Skip "("
	while ((aBracketAmount > 0) && (_pgnString[_pos] != 0))
	{
		if (_pgnString[_pos] == '(') aBracketAmount++;
		else if (_pgnString[_pos] == ')') aBracketAmount--;
		nextChar();
	}
}

// -------------------------- SkipDigits --------------------------------------
void PGNTokenizer::skipDigits()
{
	while ((_pgnString[_pos] >= '0') && (_pgnString[_pos] <= '9'))
		nextChar();
}

// -------------------------- IsSymboContinuation -----------------------------
bool PGNTokenizer::isSymbolContinuation(char ch)
{
	return 
		(((ch >= 'a') && (ch <= 'z')) || 
		((ch >= '0') && (ch <= '9')) ||
		((ch >= 'A') && (ch <= 'Z')) ||
		(ch == '_') || (ch == '+') || 
		(ch == '#') || (ch == '=') || 
		(ch == ':') || (ch == '-') || (ch == '/'));
}


// -------------------------- SkipSymbolContinuation --------------------------
void PGNTokenizer::skipSymbolContinuation()
{
	for (;;nextChar())
	{
		if (_charType[_pgnString[_pos]] != CharType::SYMBOL_CONTINUATION)
			break;
	}
}

// -------------------------- GetNextToken ------------------------------------
const string& PGNTokenizer::getNextToken()
{
	// Spaces does not belong to tokens, skip them
	skipSpaces();
	// Store first Token position
	_lastPos = _pos;
	
	char ch = _pgnString[_pos];	
	switch (ch)
	{
	case 0:
		break;
	case '[': 
	case ']': 
	case '(': 
	case ')': 
	case '<': 
	case '>': 
	case '.': 
	case '*':
		// Special single char tokens
		nextChar();
		break;
	case '!':
	case '?':
		// Move annotation
		nextChar();
		if ((_pgnString[_pos] == '?') || (_pgnString[_pos] == '!'))
			nextChar();
		break;
	case '$':
		// Scan a hyroglyph with digits
		nextChar();
		skipDigits(); 
		break;
	case '"':
		// Scan a string
		nextChar(); 
		findChar('"', false);
		nextChar();
		break;
	case ';': 
		// Comment to end of line
		skipToEOL();
		break;
	case '{': 
		findChar('}', true);
		nextChar();
		break;
	
	//case '(':
	//	SkipAnnotation();
	//	break;
	
	default:
		if (((ch >= 'a') && (ch <= 'z')) || 
			((ch >= '0') && (ch <= '9')) ||
			((ch >= 'A') && (ch <= 'Z')))
		{
			skipSymbolContinuation();
		}
		else nextChar(); // Unknown char
	}
	
	_token = _pgnString.substr(_lastPos, _pos - _lastPos); 
	return _token;
}
