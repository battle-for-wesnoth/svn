/*
* generated by Xtext
*/
grammar InternalWML;

options {
	superClass=AbstractInternalContentAssistParser;
	
}

@lexer::header {
package org.wesnoth.ui.contentassist.antlr.internal;

// Hack: Use our own Lexer superclass by means of import. 
// Currently there is no other way to specify the superclass for the lexer.
import org.eclipse.xtext.ui.editor.contentassist.antlr.internal.Lexer;
}

@parser::header {
package org.wesnoth.ui.contentassist.antlr.internal; 

import java.io.InputStream;
import org.eclipse.xtext.*;
import org.eclipse.xtext.parser.*;
import org.eclipse.xtext.parser.impl.*;
import org.eclipse.xtext.parsetree.*;
import org.eclipse.emf.ecore.util.EcoreUtil;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.parser.antlr.XtextTokenStream;
import org.eclipse.xtext.parser.antlr.XtextTokenStream.HiddenTokens;
import org.eclipse.xtext.ui.editor.contentassist.antlr.internal.AbstractInternalContentAssistParser;
import org.eclipse.xtext.ui.editor.contentassist.antlr.internal.DFA;
import org.wesnoth.services.WMLGrammarAccess;

}

@parser::members {
 
 	private WMLGrammarAccess grammarAccess;
 	
    public void setGrammarAccess(WMLGrammarAccess grammarAccess) {
    	this.grammarAccess = grammarAccess;
    }
    
    @Override
    protected Grammar getGrammar() {
    	return grammarAccess.getGrammar();
    }
    
    @Override
    protected String getValueForTokenName(String tokenName) {
    	return tokenName;
    }

}




// Entry rule entryRuleWMLRoot
entryRuleWMLRoot 
:
{ before(grammarAccess.getWMLRootRule()); }
	 ruleWMLRoot
{ after(grammarAccess.getWMLRootRule()); } 
	 EOF 
;

// Rule WMLRoot
ruleWMLRoot
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLRootAccess().getAlternatives()); }
(rule__WMLRoot__Alternatives)*
{ after(grammarAccess.getWMLRootAccess().getAlternatives()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleWMLTag
entryRuleWMLTag 
:
{ before(grammarAccess.getWMLTagRule()); }
	 ruleWMLTag
{ after(grammarAccess.getWMLTagRule()); } 
	 EOF 
;

// Rule WMLTag
ruleWMLTag
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLTagAccess().getGroup()); }
(rule__WMLTag__Group__0)
{ after(grammarAccess.getWMLTagAccess().getGroup()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleWMLKey
entryRuleWMLKey 
:
{ before(grammarAccess.getWMLKeyRule()); }
	 ruleWMLKey
{ after(grammarAccess.getWMLKeyRule()); } 
	 EOF 
;

// Rule WMLKey
ruleWMLKey
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLKeyAccess().getGroup()); }
(rule__WMLKey__Group__0)
{ after(grammarAccess.getWMLKeyAccess().getGroup()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleWMLKeyValueRule
entryRuleWMLKeyValueRule 
:
{ before(grammarAccess.getWMLKeyValueRuleRule()); }
	 ruleWMLKeyValueRule
{ after(grammarAccess.getWMLKeyValueRuleRule()); } 
	 EOF 
;

// Rule WMLKeyValueRule
ruleWMLKeyValueRule
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLKeyValueRuleAccess().getAlternatives()); }
(rule__WMLKeyValueRule__Alternatives)
{ after(grammarAccess.getWMLKeyValueRuleAccess().getAlternatives()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleWMLKeyValue
entryRuleWMLKeyValue 
:
{ before(grammarAccess.getWMLKeyValueRule()); }
	 ruleWMLKeyValue
{ after(grammarAccess.getWMLKeyValueRule()); } 
	 EOF 
;

// Rule WMLKeyValue
ruleWMLKeyValue
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLKeyValueAccess().getValueAssignment()); }
(rule__WMLKeyValue__ValueAssignment)
{ after(grammarAccess.getWMLKeyValueAccess().getValueAssignment()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleWMLMacro
entryRuleWMLMacro 
:
{ before(grammarAccess.getWMLMacroRule()); }
	 ruleWMLMacro
{ after(grammarAccess.getWMLMacroRule()); } 
	 EOF 
;

// Rule WMLMacro
ruleWMLMacro
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLMacroAccess().getNameAssignment()); }
(rule__WMLMacro__NameAssignment)
{ after(grammarAccess.getWMLMacroAccess().getNameAssignment()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleWMLLuaCode
entryRuleWMLLuaCode 
:
{ before(grammarAccess.getWMLLuaCodeRule()); }
	 ruleWMLLuaCode
{ after(grammarAccess.getWMLLuaCodeRule()); } 
	 EOF 
;

// Rule WMLLuaCode
ruleWMLLuaCode
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getWMLLuaCodeAccess().getCodeAssignment()); }
(rule__WMLLuaCode__CodeAssignment)
{ after(grammarAccess.getWMLLuaCodeAccess().getCodeAssignment()); }
)

;
finally {
	restoreStackSize(stackSize);
}



// Entry rule entryRuleT_STRING
entryRuleT_STRING 
:
{ before(grammarAccess.getT_STRINGRule()); }
	 ruleT_STRING
{ after(grammarAccess.getT_STRINGRule()); } 
	 EOF 
;

// Rule T_STRING
ruleT_STRING
    @init {
		int stackSize = keepStackSize();
    }
	:
(
{ before(grammarAccess.getT_STRINGAccess().getGroup()); }
(rule__T_STRING__Group__0)
{ after(grammarAccess.getT_STRINGAccess().getGroup()); }
)

;
finally {
	restoreStackSize(stackSize);
}




rule__WMLRoot__Alternatives
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLRootAccess().getTagsAssignment_0()); }
(rule__WMLRoot__TagsAssignment_0)
{ after(grammarAccess.getWMLRootAccess().getTagsAssignment_0()); }
)

    |(
{ before(grammarAccess.getWMLRootAccess().getMacrosAssignment_1()); }
(rule__WMLRoot__MacrosAssignment_1)
{ after(grammarAccess.getWMLRootAccess().getMacrosAssignment_1()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Alternatives_4
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getTagsAssignment_4_0()); }
(rule__WMLTag__TagsAssignment_4_0)
{ after(grammarAccess.getWMLTagAccess().getTagsAssignment_4_0()); }
)

    |(
{ before(grammarAccess.getWMLTagAccess().getKeysAssignment_4_1()); }
(rule__WMLTag__KeysAssignment_4_1)
{ after(grammarAccess.getWMLTagAccess().getKeysAssignment_4_1()); }
)

    |(
{ before(grammarAccess.getWMLTagAccess().getMacrosAssignment_4_2()); }
(rule__WMLTag__MacrosAssignment_4_2)
{ after(grammarAccess.getWMLTagAccess().getMacrosAssignment_4_2()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKeyValueRule__Alternatives
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyValueRuleAccess().getWMLKeyValueParserRuleCall_0()); }
	ruleWMLKeyValue
{ after(grammarAccess.getWMLKeyValueRuleAccess().getWMLKeyValueParserRuleCall_0()); }
)

    |(
{ before(grammarAccess.getWMLKeyValueRuleAccess().getWMLMacroParserRuleCall_1()); }
	ruleWMLMacro
{ after(grammarAccess.getWMLKeyValueRuleAccess().getWMLMacroParserRuleCall_1()); }
)

    |(
{ before(grammarAccess.getWMLKeyValueRuleAccess().getWMLLuaCodeParserRuleCall_2()); }
	ruleWMLLuaCode
{ after(grammarAccess.getWMLKeyValueRuleAccess().getWMLLuaCodeParserRuleCall_2()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKeyValue__ValueAlternatives_0
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyValueAccess().getValueIDTerminalRuleCall_0_0()); }
	RULE_ID
{ after(grammarAccess.getWMLKeyValueAccess().getValueIDTerminalRuleCall_0_0()); }
)

    |(
{ before(grammarAccess.getWMLKeyValueAccess().getValueSTRINGTerminalRuleCall_0_1()); }
	RULE_STRING
{ after(grammarAccess.getWMLKeyValueAccess().getValueSTRINGTerminalRuleCall_0_1()); }
)

    |(
{ before(grammarAccess.getWMLKeyValueAccess().getValueANY_OTHERTerminalRuleCall_0_2()); }
	RULE_ANY_OTHER
{ after(grammarAccess.getWMLKeyValueAccess().getValueANY_OTHERTerminalRuleCall_0_2()); }
)

    |(
{ before(grammarAccess.getWMLKeyValueAccess().getValueT_STRINGParserRuleCall_0_3()); }
	ruleT_STRING
{ after(grammarAccess.getWMLKeyValueAccess().getValueT_STRINGParserRuleCall_0_3()); }
)

;
finally {
	restoreStackSize(stackSize);
}



rule__WMLTag__Group__0
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__0__Impl
	rule__WMLTag__Group__1
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__0__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getLeftSquareBracketKeyword_0()); }

	'[' 

{ after(grammarAccess.getWMLTagAccess().getLeftSquareBracketKeyword_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__1
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__1__Impl
	rule__WMLTag__Group__2
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__1__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getPlusAssignment_1()); }
(rule__WMLTag__PlusAssignment_1)?
{ after(grammarAccess.getWMLTagAccess().getPlusAssignment_1()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__2
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__2__Impl
	rule__WMLTag__Group__3
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__2__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getNameAssignment_2()); }
(rule__WMLTag__NameAssignment_2)
{ after(grammarAccess.getWMLTagAccess().getNameAssignment_2()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__3
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__3__Impl
	rule__WMLTag__Group__4
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__3__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getRightSquareBracketKeyword_3()); }

	']' 

{ after(grammarAccess.getWMLTagAccess().getRightSquareBracketKeyword_3()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__4
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__4__Impl
	rule__WMLTag__Group__5
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__4__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getAlternatives_4()); }
(rule__WMLTag__Alternatives_4)*
{ after(grammarAccess.getWMLTagAccess().getAlternatives_4()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__5
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__5__Impl
	rule__WMLTag__Group__6
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__5__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getLeftSquareBracketSolidusKeyword_5()); }

	'[/' 

{ after(grammarAccess.getWMLTagAccess().getLeftSquareBracketSolidusKeyword_5()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__6
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__6__Impl
	rule__WMLTag__Group__7
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__6__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getEndNameAssignment_6()); }
(rule__WMLTag__EndNameAssignment_6)
{ after(grammarAccess.getWMLTagAccess().getEndNameAssignment_6()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLTag__Group__7
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLTag__Group__7__Impl
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__Group__7__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getRightSquareBracketKeyword_7()); }

	']' 

{ after(grammarAccess.getWMLTagAccess().getRightSquareBracketKeyword_7()); }
)

;
finally {
	restoreStackSize(stackSize);
}


















rule__WMLKey__Group__0
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLKey__Group__0__Impl
	rule__WMLKey__Group__1
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__Group__0__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getNameAssignment_0()); }
(rule__WMLKey__NameAssignment_0)
{ after(grammarAccess.getWMLKeyAccess().getNameAssignment_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLKey__Group__1
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLKey__Group__1__Impl
	rule__WMLKey__Group__2
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__Group__1__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getEqualsSignKeyword_1()); }

	'=' 

{ after(grammarAccess.getWMLKeyAccess().getEqualsSignKeyword_1()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLKey__Group__2
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLKey__Group__2__Impl
	rule__WMLKey__Group__3
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__Group__2__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
(
{ before(grammarAccess.getWMLKeyAccess().getValueAssignment_2()); }
(rule__WMLKey__ValueAssignment_2)
{ after(grammarAccess.getWMLKeyAccess().getValueAssignment_2()); }
)
(
{ before(grammarAccess.getWMLKeyAccess().getValueAssignment_2()); }
(rule__WMLKey__ValueAssignment_2)*
{ after(grammarAccess.getWMLKeyAccess().getValueAssignment_2()); }
)
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLKey__Group__3
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLKey__Group__3__Impl
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__Group__3__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getGroup_3()); }
(rule__WMLKey__Group_3__0)*
{ after(grammarAccess.getWMLKeyAccess().getGroup_3()); }
)

;
finally {
	restoreStackSize(stackSize);
}










rule__WMLKey__Group_3__0
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLKey__Group_3__0__Impl
	rule__WMLKey__Group_3__1
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__Group_3__0__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getPlusSignKeyword_3_0()); }

	'+' 

{ after(grammarAccess.getWMLKeyAccess().getPlusSignKeyword_3_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__WMLKey__Group_3__1
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__WMLKey__Group_3__1__Impl
;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__Group_3__1__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getExtraMacrosAssignment_3_1()); }
(rule__WMLKey__ExtraMacrosAssignment_3_1)
{ after(grammarAccess.getWMLKeyAccess().getExtraMacrosAssignment_3_1()); }
)

;
finally {
	restoreStackSize(stackSize);
}






rule__T_STRING__Group__0
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__T_STRING__Group__0__Impl
	rule__T_STRING__Group__1
;
finally {
	restoreStackSize(stackSize);
}

rule__T_STRING__Group__0__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getT_STRINGAccess().get_Keyword_0()); }

	'_' 

{ after(grammarAccess.getT_STRINGAccess().get_Keyword_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}


rule__T_STRING__Group__1
    @init {
		int stackSize = keepStackSize();
    }
:
	rule__T_STRING__Group__1__Impl
;
finally {
	restoreStackSize(stackSize);
}

rule__T_STRING__Group__1__Impl
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getT_STRINGAccess().getSTRINGTerminalRuleCall_1()); }
	RULE_STRING
{ after(grammarAccess.getT_STRINGAccess().getSTRINGTerminalRuleCall_1()); }
)

;
finally {
	restoreStackSize(stackSize);
}







rule__WMLRoot__TagsAssignment_0
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLRootAccess().getTagsWMLTagParserRuleCall_0_0()); }
	ruleWMLTag{ after(grammarAccess.getWMLRootAccess().getTagsWMLTagParserRuleCall_0_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLRoot__MacrosAssignment_1
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLRootAccess().getMacrosWMLMacroParserRuleCall_1_0()); }
	ruleWMLMacro{ after(grammarAccess.getWMLRootAccess().getMacrosWMLMacroParserRuleCall_1_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__PlusAssignment_1
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getPlusPlusSignKeyword_1_0()); }
(
{ before(grammarAccess.getWMLTagAccess().getPlusPlusSignKeyword_1_0()); }

	'+' 

{ after(grammarAccess.getWMLTagAccess().getPlusPlusSignKeyword_1_0()); }
)

{ after(grammarAccess.getWMLTagAccess().getPlusPlusSignKeyword_1_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__NameAssignment_2
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getNameIDTerminalRuleCall_2_0()); }
	RULE_ID{ after(grammarAccess.getWMLTagAccess().getNameIDTerminalRuleCall_2_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__TagsAssignment_4_0
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getTagsWMLTagParserRuleCall_4_0_0()); }
	ruleWMLTag{ after(grammarAccess.getWMLTagAccess().getTagsWMLTagParserRuleCall_4_0_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__KeysAssignment_4_1
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getKeysWMLKeyParserRuleCall_4_1_0()); }
	ruleWMLKey{ after(grammarAccess.getWMLTagAccess().getKeysWMLKeyParserRuleCall_4_1_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__MacrosAssignment_4_2
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getMacrosWMLMacroParserRuleCall_4_2_0()); }
	ruleWMLMacro{ after(grammarAccess.getWMLTagAccess().getMacrosWMLMacroParserRuleCall_4_2_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLTag__EndNameAssignment_6
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLTagAccess().getEndNameIDTerminalRuleCall_6_0()); }
	RULE_ID{ after(grammarAccess.getWMLTagAccess().getEndNameIDTerminalRuleCall_6_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__NameAssignment_0
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getNameIDTerminalRuleCall_0_0()); }
	RULE_ID{ after(grammarAccess.getWMLKeyAccess().getNameIDTerminalRuleCall_0_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__ValueAssignment_2
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getValueWMLKeyValueRuleParserRuleCall_2_0()); }
	ruleWMLKeyValueRule{ after(grammarAccess.getWMLKeyAccess().getValueWMLKeyValueRuleParserRuleCall_2_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKey__ExtraMacrosAssignment_3_1
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyAccess().getExtraMacrosWMLMacroParserRuleCall_3_1_0()); }
	ruleWMLMacro{ after(grammarAccess.getWMLKeyAccess().getExtraMacrosWMLMacroParserRuleCall_3_1_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLKeyValue__ValueAssignment
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLKeyValueAccess().getValueAlternatives_0()); }
(rule__WMLKeyValue__ValueAlternatives_0)
{ after(grammarAccess.getWMLKeyValueAccess().getValueAlternatives_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLMacro__NameAssignment
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLMacroAccess().getNameRULE_MACROTerminalRuleCall_0()); }
	RULE_RULE_MACRO{ after(grammarAccess.getWMLMacroAccess().getNameRULE_MACROTerminalRuleCall_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}

rule__WMLLuaCode__CodeAssignment
    @init {
		int stackSize = keepStackSize();
    }
:
(
{ before(grammarAccess.getWMLLuaCodeAccess().getCodeRULE_LUA_CODETerminalRuleCall_0()); }
	RULE_RULE_LUA_CODE{ after(grammarAccess.getWMLLuaCodeAccess().getCodeRULE_LUA_CODETerminalRuleCall_0()); }
)

;
finally {
	restoreStackSize(stackSize);
}


RULE_RULE_LUA_CODE : '<<' ( options {greedy=false;} : . )*'>>';

RULE_RULE_MACRO : '{' ( options {greedy=false;} : . )*'}';

RULE_SL_COMMENT : '#' ~(('\n'|'\r'))* ('\r'? '\n')?;

RULE_ID : ('a'..'z'|'A'..'Z'|'0'..'9'|'_')+;

RULE_STRING : '"' ('\\' ('b'|'t'|'n'|'f'|'r'|'"'|'\''|'\\')|~(('\\'|'"')))* '"';

RULE_WS : (' '|'\t'|'\r'|'\n')+;

RULE_ANY_OTHER : .;


