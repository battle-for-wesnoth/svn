/*
* generated by Xtext
*/
package org.wesnoth.ui.contentassist.antlr;

import java.util.Collection;
import java.util.Map;
import java.util.HashMap;

import org.antlr.runtime.RecognitionException;
import org.eclipse.xtext.AbstractElement;
import org.eclipse.xtext.ui.editor.contentassist.antlr.AbstractContentAssistParser;
import org.eclipse.xtext.ui.editor.contentassist.antlr.FollowElement;
import org.eclipse.xtext.ui.editor.contentassist.antlr.internal.AbstractInternalContentAssistParser;

import com.google.inject.Inject;

import org.wesnoth.services.WMLGrammarAccess;

public class WMLParser extends AbstractContentAssistParser {
	
	@Inject
	private WMLGrammarAccess grammarAccess;
	
	private Map<AbstractElement, String> nameMappings;
	
	@Override
	protected org.wesnoth.ui.contentassist.antlr.internal.InternalWMLParser createParser() {
		org.wesnoth.ui.contentassist.antlr.internal.InternalWMLParser result = new org.wesnoth.ui.contentassist.antlr.internal.InternalWMLParser(null);
		result.setGrammarAccess(grammarAccess);
		return result;
	}
	
	@Override
	protected String getRuleName(AbstractElement element) {
		if (nameMappings == null) {
			nameMappings = new HashMap<AbstractElement, String>() {
				private static final long serialVersionUID = 1L;
				{
					put(grammarAccess.getWMLKeyAccess().getEolAlternatives_4_0(), "rule__WMLKey__EolAlternatives_4_0");
					put(grammarAccess.getWMLKeyValueAccess().getAlternatives(), "rule__WMLKeyValue__Alternatives");
					put(grammarAccess.getWMLMacroCallParameterAccess().getAlternatives(), "rule__WMLMacroCallParameter__Alternatives");
					put(grammarAccess.getWMLPreprocIFAccess().getNameAlternatives_0_0(), "rule__WMLPreprocIF__NameAlternatives_0_0");
					put(grammarAccess.getWMLRootExpressionAccess().getAlternatives(), "rule__WMLRootExpression__Alternatives");
					put(grammarAccess.getWMLExpressionAccess().getAlternatives(), "rule__WMLExpression__Alternatives");
					put(grammarAccess.getWMLValuedExpressionAccess().getAlternatives(), "rule__WMLValuedExpression__Alternatives");
					put(grammarAccess.getWMLMacroParameterAccess().getAlternatives(), "rule__WMLMacroParameter__Alternatives");
					put(grammarAccess.getWMLValueAccess().getValueAlternatives_0(), "rule__WMLValue__ValueAlternatives_0");
					put(grammarAccess.getMacroTokensAccess().getValueAlternatives_0(), "rule__MacroTokens__ValueAlternatives_0");
					put(grammarAccess.getWMLTagAccess().getGroup(), "rule__WMLTag__Group__0");
					put(grammarAccess.getWMLKeyAccess().getGroup(), "rule__WMLKey__Group__0");
					put(grammarAccess.getWMLKeyAccess().getGroup_3(), "rule__WMLKey__Group_3__0");
					put(grammarAccess.getWMLMacroCallAccess().getGroup(), "rule__WMLMacroCall__Group__0");
					put(grammarAccess.getWMLArrayCallAccess().getGroup(), "rule__WMLArrayCall__Group__0");
					put(grammarAccess.getWMLMacroDefineAccess().getGroup(), "rule__WMLMacroDefine__Group__0");
					put(grammarAccess.getWMLPreprocIFAccess().getGroup(), "rule__WMLPreprocIF__Group__0");
					put(grammarAccess.getWMLPreprocIFAccess().getGroup_2(), "rule__WMLPreprocIF__Group_2__0");
					put(grammarAccess.getWMLRootAccess().getExpressionsAssignment(), "rule__WMLRoot__ExpressionsAssignment");
					put(grammarAccess.getWMLTagAccess().getPlusAssignment_1(), "rule__WMLTag__PlusAssignment_1");
					put(grammarAccess.getWMLTagAccess().getNameAssignment_2(), "rule__WMLTag__NameAssignment_2");
					put(grammarAccess.getWMLTagAccess().getExpressionsAssignment_4(), "rule__WMLTag__ExpressionsAssignment_4");
					put(grammarAccess.getWMLTagAccess().getEndNameAssignment_6(), "rule__WMLTag__EndNameAssignment_6");
					put(grammarAccess.getWMLKeyAccess().getNameAssignment_0(), "rule__WMLKey__NameAssignment_0");
					put(grammarAccess.getWMLKeyAccess().getValuesAssignment_2(), "rule__WMLKey__ValuesAssignment_2");
					put(grammarAccess.getWMLKeyAccess().getValuesAssignment_3_3(), "rule__WMLKey__ValuesAssignment_3_3");
					put(grammarAccess.getWMLKeyAccess().getEolAssignment_4(), "rule__WMLKey__EolAssignment_4");
					put(grammarAccess.getWMLMacroCallAccess().getPointAssignment_1(), "rule__WMLMacroCall__PointAssignment_1");
					put(grammarAccess.getWMLMacroCallAccess().getRelativeAssignment_2(), "rule__WMLMacroCall__RelativeAssignment_2");
					put(grammarAccess.getWMLMacroCallAccess().getNameAssignment_3(), "rule__WMLMacroCall__NameAssignment_3");
					put(grammarAccess.getWMLMacroCallAccess().getParametersAssignment_4(), "rule__WMLMacroCall__ParametersAssignment_4");
					put(grammarAccess.getWMLArrayCallAccess().getValueAssignment_1(), "rule__WMLArrayCall__ValueAssignment_1");
					put(grammarAccess.getWMLMacroDefineAccess().getNameAssignment_0(), "rule__WMLMacroDefine__NameAssignment_0");
					put(grammarAccess.getWMLMacroDefineAccess().getExpressionsAssignment_1(), "rule__WMLMacroDefine__ExpressionsAssignment_1");
					put(grammarAccess.getWMLMacroDefineAccess().getEndNameAssignment_2(), "rule__WMLMacroDefine__EndNameAssignment_2");
					put(grammarAccess.getWMLPreprocIFAccess().getNameAssignment_0(), "rule__WMLPreprocIF__NameAssignment_0");
					put(grammarAccess.getWMLPreprocIFAccess().getExpressionsAssignment_1(), "rule__WMLPreprocIF__ExpressionsAssignment_1");
					put(grammarAccess.getWMLPreprocIFAccess().getElsesAssignment_2_0(), "rule__WMLPreprocIF__ElsesAssignment_2_0");
					put(grammarAccess.getWMLPreprocIFAccess().getElseExpressionsAssignment_2_1(), "rule__WMLPreprocIF__ElseExpressionsAssignment_2_1");
					put(grammarAccess.getWMLPreprocIFAccess().getEndNameAssignment_3(), "rule__WMLPreprocIF__EndNameAssignment_3");
					put(grammarAccess.getWMLTextdomainAccess().getNameAssignment(), "rule__WMLTextdomain__NameAssignment");
					put(grammarAccess.getWMLLuaCodeAccess().getValueAssignment(), "rule__WMLLuaCode__ValueAssignment");
					put(grammarAccess.getWMLValueAccess().getValueAssignment(), "rule__WMLValue__ValueAssignment");
					put(grammarAccess.getMacroTokensAccess().getValueAssignment(), "rule__MacroTokens__ValueAssignment");
				}
			};
		}
		return nameMappings.get(element);
	}
	
	@Override
	protected Collection<FollowElement> getFollowElements(AbstractInternalContentAssistParser parser) {
		try {
			org.wesnoth.ui.contentassist.antlr.internal.InternalWMLParser typedParser = (org.wesnoth.ui.contentassist.antlr.internal.InternalWMLParser) parser;
			typedParser.entryRuleWMLRoot();
			return typedParser.getFollowElements();
		} catch(RecognitionException ex) {
			throw new RuntimeException(ex);
		}		
	}
	
	@Override
	protected String[] getInitialHiddenTokens() {
		return new String[] { "RULE_EOL", "RULE_WS", "RULE_SL_COMMENT" };
	}
	
	public WMLGrammarAccess getGrammarAccess() {
		return this.grammarAccess;
	}
	
	public void setGrammarAccess(WMLGrammarAccess grammarAccess) {
		this.grammarAccess = grammarAccess;
	}
}
