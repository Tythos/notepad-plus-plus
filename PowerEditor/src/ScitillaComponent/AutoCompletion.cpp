// This file is part of Notepad++ project
// Copyright (C)2003 Don HO <don.h@free.fr>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// Note that the GPL places important restrictions on "derived works", yet
// it does not provide a detailed definition of that term.  To avoid      
// misunderstandings, we consider an application to constitute a          
// "derivative work" for the purpose of this license if it does any of the
// following:                                                             
// 1. Integrates source code from Notepad++.
// 2. Integrates/includes/aggregates Notepad++ into a proprietary executable
//    installer, such as those produced by InstallShield.
// 3. Links to a library or executes a program that does any of the above.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "precompiledHeaders.h"

#include "AutoCompletion.h"
#include "Notepad_plus_msgs.h"

static bool isInList(generic_string word, const vector<generic_string> & wordArray)
{
	for (size_t i = 0, len = wordArray.size(); i < len; ++i)
		if (wordArray[i] == word)
			return true;
	return false;
};

bool AutoCompletion::showAutoComplete() {
	if (!_funcCompletionActive)
		return false;

	int curPos = int(_pEditView->execute(SCI_GETCURRENTPOS));
	int line = _pEditView->getCurrentLineNumber();
	int startLinePos = int(_pEditView->execute(SCI_POSITIONFROMLINE, line ));
	int startWordPos = startLinePos;

	int len = curPos-startLinePos;
	char * lineBuffer = new char[len+1];
	_pEditView->getText(lineBuffer, startLinePos, curPos);

	int offset = len-1;
	int nrChars = 0;
	char c;
	while (offset>=0)
	{
		c = lineBuffer[offset];
		if (isalnum(c) || c == '_') {
			++nrChars;
		} else {
			break;
		}
		--offset;
		
	}
	startWordPos = curPos-nrChars;

	_pEditView->execute(SCI_AUTOCSETSEPARATOR, WPARAM('\n'));
	_pEditView->execute(SCI_AUTOCSETIGNORECASE, _ignoreCase);
	_pEditView->showAutoComletion(curPos - startWordPos, _keyWords.c_str());

	_activeCompletion = CompletionAuto;
	return true;
}

bool AutoCompletion::showWordComplete(bool autoInsert) 
{
	int curPos = int(_pEditView->execute(SCI_GETCURRENTPOS));
	int startPos = int(_pEditView->execute(SCI_WORDSTARTPOSITION, curPos, true));

	if (curPos == startPos)
		return false;

	const size_t bufSize = 256;
	size_t len = (curPos > startPos)?(curPos - startPos):(startPos - curPos);
	if (len >= bufSize)
		return false;

	TCHAR beginChars[bufSize];

	_pEditView->getGenericText(beginChars, bufSize, startPos, curPos);

	generic_string expr(TEXT("\\<"));
	expr += beginChars;
	expr += TEXT("[^ \\t\\n\\r.,;:\"()=<>'+!\\[\\]]*");

	int docLength = int(_pEditView->execute(SCI_GETLENGTH));

	int flags = SCFIND_WORDSTART | SCFIND_MATCHCASE | SCFIND_REGEXP | SCFIND_POSIX;

	_pEditView->execute(SCI_SETSEARCHFLAGS, flags);
	vector<generic_string> wordArray;
	int posFind = _pEditView->searchInTarget(expr.c_str(), expr.length(), 0, docLength);

	while (posFind != -1)
	{
		int wordStart = int(_pEditView->execute(SCI_GETTARGETSTART));
		int wordEnd = int(_pEditView->execute(SCI_GETTARGETEND));
		
		size_t foundTextLen = wordEnd - wordStart;

		if (foundTextLen < bufSize)
		{
			TCHAR w[bufSize];
			_pEditView->getGenericText(w, bufSize, wordStart, wordEnd);

			if (lstrcmp(w, beginChars) != 0)
				if (!isInList(w, wordArray))
					wordArray.push_back(w);
		}
		posFind = _pEditView->searchInTarget(expr.c_str(), expr.length(), wordEnd, docLength);
	}
	if (wordArray.size() == 0) return false;

	if (wordArray.size() == 1 && autoInsert) 
	{
		_pEditView->replaceTargetRegExMode(wordArray[0].c_str(), startPos, curPos);
		_pEditView->execute(SCI_GOTOPOS, startPos + wordArray[0].length());
		return true;
	}

	sort(wordArray.begin(), wordArray.end());
	generic_string words(TEXT(""));

	for (size_t i = 0, len = wordArray.size(); i < len; ++i)
	{
		words += wordArray[i];
		if (i != wordArray.size()-1)
			words += TEXT(" ");
	}

	// UNICODE TO DO
	_pEditView->execute(SCI_AUTOCSETSEPARATOR, WPARAM(' '));
	_pEditView->execute(SCI_AUTOCSETIGNORECASE, _ignoreCase);
	_pEditView->showAutoComletion(curPos - startPos, words.c_str());

	_activeCompletion = CompletionWord;
	return true;
}

bool AutoCompletion::showFunctionComplete() {
	if (!_funcCompletionActive)
		return false;

	if (_funcCalltip.updateCalltip(0, true)) {
		_activeCompletion = CompletionFunc;
		return true;
	}
	return false;
}

void AutoCompletion::getCloseTag(char *closeTag, size_t closeTagSize, size_t caretPos)
{
	int flags = SCFIND_REGEXP | SCFIND_POSIX;
	_pEditView->execute(SCI_SETSEARCHFLAGS, flags);
	TCHAR tag2find[] = TEXT("<[^\\s>]*");
	int targetStart = _pEditView->searchInTarget(tag2find, lstrlen(tag2find), caretPos, 0);
	
	if (targetStart == -1 || targetStart == -2)
		return;

	int targetEnd = int(_pEditView->execute(SCI_GETTARGETEND));
	int foundTextLen = targetEnd - targetStart;
	if (foundTextLen < 2) // "<>" will be ignored
		return;

	if (size_t(foundTextLen) > closeTagSize - 2) // buffer size is not large enough. -2 for '/' & '\0'
		return;

	char tagHead[3];
	_pEditView->getText(tagHead, targetStart, targetStart+2);

	if (tagHead[1] == '/') // "</toto>" will be ignored
		return;

	char tagTail[2];
	_pEditView->getText(tagTail, caretPos-2, caretPos-1);

	if (tagTail[0] == '/') // "<toto/>" and "<toto arg="0" />" will be ignored
		return;

	closeTag[0] = '<';
	closeTag[1] = '/';
	_pEditView->getText(closeTag + 2, targetStart + 1, targetEnd);
	closeTag[foundTextLen+1] = '>';
	closeTag[foundTextLen+2] = '\0'; 
}

void AutoCompletion::insertMatchedChars(int character, const MatchedPairConf & matchedPairConf)
{
	const vector< pair<char, char> > & matchedPairs = matchedPairConf._matchedPairs;
	int caretPos = _pEditView->execute(SCI_GETCURRENTPOS);
	char *matchedChars = NULL;

	// User defined matched pairs should be checked firstly
	for (size_t i = 0, len = matchedPairs.size(); i < len; ++i)
	{
		if (int(matchedPairs[i].first) == character)
		{
			char userMatchedChar[2] = {'\0', '\0'};
			userMatchedChar[0] = matchedPairs[i].second;
			_pEditView->execute(SCI_INSERTTEXT, caretPos, (LPARAM)userMatchedChar);
			return;
		}
	}

	// if there's no user defined matched pair found, continue to check notepad++'s one

	const size_t closeTagLen = 256;
	char closeTag[closeTagLen];
	closeTag[0] = '\0';
	switch (character)
	{
		case int('('):
			if (matchedPairConf._doParentheses)
				matchedChars = ")"; 
		break;
		case int('['):
			if (matchedPairConf._doBrackets)
				matchedChars = "]";
		break;
		case int('{'):
			if (matchedPairConf._doCurlyBrackets)
				matchedChars = "}";
		break;
		case int('"'):
			if (matchedPairConf._doDoubleQuotes)
				matchedChars = "\"";
		break;
		case int('\''):
			if (matchedPairConf._doQuotes)
				matchedChars = "'";
		break;
		case int('>'):
		{
			if (matchedPairConf._doHtmlXmlTag && (_curLang == L_HTML || _curLang == L_XML))
			{
				getCloseTag(closeTag, closeTagLen, caretPos);
				if (closeTag[0] != '\0')
					matchedChars = closeTag;
			}
		}
		break;
	}

	if (matchedChars)
		_pEditView->execute(SCI_INSERTTEXT, caretPos, (LPARAM)matchedChars);
}


void AutoCompletion::update(int character)
{
	const NppGUI & nppGUI = NppParameters::getInstance()->getNppGUI();
	if (!_funcCompletionActive && nppGUI._autocStatus == nppGUI.autoc_func)
		return;

	if (nppGUI._funcParams || _funcCalltip.isVisible()) {
		if (_funcCalltip.updateCalltip(character)) {	//calltip visible because triggered by autocomplete, set mode
			_activeCompletion = CompletionFunc;
			return;	//only return in case of success, else autocomplete
		}
	}

	if (!character)
		return;

	//If autocomplete already active, let Scintilla handle it
	if (_pEditView->execute(SCI_AUTOCACTIVE) != 0)
		return;

	const int wordSize = 64;
	TCHAR s[wordSize];
	_pEditView->getWordToCurrentPos(s, wordSize);
	
	if (lstrlen(s) >= int(nppGUI._autocFromLen))
	{
		if (nppGUI._autocStatus == nppGUI.autoc_word)
		{
			// Walk around - to avoid the crash under Chinese Windows7 ANSI doc mode
			if (!_pEditView->isCJK())
			{
				showWordComplete(false);
			}
			else
			{
				if ((_pEditView->getCurrentBuffer())->getUnicodeMode() != uni8Bit)
					showWordComplete(false);
			}
		}
		else if (nppGUI._autocStatus == nppGUI.autoc_func)
			showAutoComplete();

		/*
		if (nppGUI._autocStatus == nppGUI.autoc_word)
			showWordComplete(false);
		else if (nppGUI._autocStatus == nppGUI.autoc_func)
			showAutoComplete();
		*/
	}
}

void AutoCompletion::callTipClick(int direction) {
	if (!_funcCompletionActive)
		return;

	if (direction == 1) {
		_funcCalltip.showPrevOverload();
	} else if (direction == 2) {
		_funcCalltip.showNextOverload();
	}
}

bool AutoCompletion::setLanguage(LangType language) {
	if (_curLang == language)
		return true;
	_curLang = language;

	TCHAR path[MAX_PATH];
	::GetModuleFileName(NULL, path, MAX_PATH);
	PathRemoveFileSpec(path);
	lstrcat(path, TEXT("\\plugins\\APIs\\"));
	lstrcat(path, getApiFileName());
	lstrcat(path, TEXT(".xml"));

	if (_pXmlFile)
		delete _pXmlFile;

	_pXmlFile = new TiXmlDocument(path);
	_funcCompletionActive = _pXmlFile->LoadFile();

	TiXmlNode * pAutoNode = NULL;
	if (_funcCompletionActive) {
		_funcCompletionActive = false;	//safety
		TiXmlNode * pNode = _pXmlFile->FirstChild(TEXT("NotepadPlus"));
		if (!pNode)
			return false;
		pAutoNode = pNode = pNode->FirstChildElement(TEXT("AutoComplete"));
		if (!pNode)
			return false;
		pNode = pNode->FirstChildElement(TEXT("KeyWord"));
		if (!pNode)
			return false;
		_pXmlKeyword = reinterpret_cast<TiXmlElement *>(pNode);
		if (!_pXmlKeyword)
			return false;
		_funcCompletionActive = true;
	}

	if(_funcCompletionActive) //try setting up environment
    {
		//setup defaults
		_ignoreCase = true;
		_funcCalltip._start = '(';
		_funcCalltip._stop = ')';
		_funcCalltip._param = ',';
		_funcCalltip._terminal = ';';
		_funcCalltip._ignoreCase = true;
        _funcCalltip._additionalWordChar = TEXT("");

		TiXmlElement * pElem = pAutoNode->FirstChildElement(TEXT("Environment"));
		if (pElem) 
        {	
			const TCHAR * val = 0;
			val = pElem->Attribute(TEXT("ignoreCase"));
			if (val && !lstrcmp(val, TEXT("no"))) {
				_ignoreCase = false;
				_funcCalltip._ignoreCase = false;
			}
			val = pElem->Attribute(TEXT("startFunc"));
			if (val && val[0])
				_funcCalltip._start = val[0];
			val = pElem->Attribute(TEXT("stopFunc"));
			if (val && val[0])
				_funcCalltip._stop = val[0];
			val = pElem->Attribute(TEXT("paramSeparator"));
			if (val && val[0])
				_funcCalltip._param = val[0];
			val = pElem->Attribute(TEXT("terminal"));
			if (val && val[0])
				_funcCalltip._terminal = val[0];
			val = pElem->Attribute(TEXT("additionalWordChar"));
			if (val && val[0])
                _funcCalltip._additionalWordChar = val;
		}
	}

	if (_funcCompletionActive) {
		_funcCalltip.setLanguageXML(_pXmlKeyword);
	} else {
		_funcCalltip.setLanguageXML(NULL);
	}

	_keyWords = TEXT("");
	if (_funcCompletionActive) {	//Cache the keywords
		//Iterate through all keywords
		TiXmlElement *funcNode = _pXmlKeyword;
		const TCHAR * name = NULL;
		for (; funcNode; funcNode = funcNode->NextSiblingElement(TEXT("KeyWord")) ) {
			name = funcNode->Attribute(TEXT("name"));
			if (!name)		//malformed node
				continue;
			_keyWords.append(name);
			_keyWords.append(TEXT("\n"));
		}
	}
	return _funcCompletionActive;
}

const TCHAR * AutoCompletion::getApiFileName() {
	if (_curLang == L_USER)
	{
		Buffer * currentBuf = _pEditView->getCurrentBuffer();
		if (currentBuf->isUserDefineLangExt())
		{
			return currentBuf->getUserDefineLangName();
		}
	}

	if (_curLang >= L_EXTERNAL && _curLang < NppParameters::getInstance()->L_END)
		return NppParameters::getInstance()->getELCFromIndex(_curLang - L_EXTERNAL)._name;

	if (_curLang > L_EXTERNAL)
        _curLang = L_TEXT;

	return ScintillaEditView::langNames[_curLang].lexerName;

}
