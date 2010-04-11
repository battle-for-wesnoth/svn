/*
* generated by Xtext
*/
grammar InternalWml;

options {
	superClass=AbstractInternalAntlrParser;
	
}

@lexer::header {
package org.wesnoth.parser.antlr.internal;

// Hack: Use our own Lexer superclass by means of import. 
// Currently there is no other way to specify the superclass for the lexer.
import org.eclipse.xtext.parser.antlr.Lexer;
}

@parser::header {
package org.wesnoth.parser.antlr.internal; 

import java.io.InputStream;
import org.eclipse.xtext.*;
import org.eclipse.xtext.parser.*;
import org.eclipse.xtext.parser.impl.*;
import org.eclipse.xtext.parsetree.*;
import org.eclipse.emf.ecore.util.EcoreUtil;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.parser.antlr.AbstractInternalAntlrParser;
import org.eclipse.xtext.parser.antlr.XtextTokenStream;
import org.eclipse.xtext.parser.antlr.XtextTokenStream.HiddenTokens;
import org.eclipse.xtext.parser.antlr.AntlrDatatypeRuleToken;
import org.eclipse.xtext.conversion.ValueConverterException;
import org.wesnoth.services.WmlGrammarAccess;

}

@parser::members {
 
 	private WmlGrammarAccess grammarAccess;
 	
    public InternalWmlParser(TokenStream input, IAstFactory factory, WmlGrammarAccess grammarAccess) {
        this(input);
        this.factory = factory;
        registerRules(grammarAccess.getGrammar());
        this.grammarAccess = grammarAccess;
    }
    
    @Override
    protected InputStream getTokenFile() {
    	ClassLoader classLoader = getClass().getClassLoader();
    	return classLoader.getResourceAsStream("org/wesnoth/parser/antlr/internal/InternalWml.tokens");
    }
    
    @Override
    protected String getFirstRuleName() {
    	return "Model";	
   	} 
}

@rulecatch { 
    catch (RecognitionException re) { 
        recover(input,re); 
        appendSkippedTokens();
    } 
}




// Entry rule entryRuleModel
entryRuleModel returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getModelRule(), currentNode); }
	 iv_ruleModel=ruleModel 
	 { $current=$iv_ruleModel.current; } 
	 EOF 
;

// Rule Model
ruleModel returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
((	
	
	    
	    { 
	        currentNode=createCompositeNode(grammarAccess.getModelAccess().getImportsImportParserRuleCall_0_0(), currentNode); 
	    }
	    lv_imports_0=ruleImport 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getModelRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        
	        try {
	       		add($current, "imports", lv_imports_0, "Import", currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }
	
)*(	
	
	    
	    { 
	        currentNode=createCompositeNode(grammarAccess.getModelAccess().getElementsTypeParserRuleCall_1_0(), currentNode); 
	    }
	    lv_elements_1=ruleType 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getModelRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        
	        try {
	       		add($current, "elements", lv_elements_1, "Type", currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }
	
)*);





// Entry rule entryRuleImport
entryRuleImport returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getImportRule(), currentNode); }
	 iv_ruleImport=ruleImport 
	 { $current=$iv_ruleImport.current; } 
	 EOF 
;

// Rule Import
ruleImport returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
('import' 
    {
        createLeafNode(grammarAccess.getImportAccess().getImportKeyword_0(), null); 
    }
(	
	
	    lv_importURI_1=	RULE_STRING
	{
		createLeafNode(grammarAccess.getImportAccess().getImportURISTRINGTerminalRuleCall_1_0(), "importURI"); 
	}
 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getImportRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "importURI", lv_importURI_1, "STRING", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }
	
));





// Entry rule entryRuleType
entryRuleType returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getTypeRule(), currentNode); }
	 iv_ruleType=ruleType 
	 { $current=$iv_ruleType.current; } 
	 EOF 
;

// Rule Type
ruleType returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
(
    { 
        currentNode=createCompositeNode(grammarAccess.getTypeAccess().getSimpleTypeParserRuleCall_0(), currentNode); 
    }
    this_SimpleType_0=ruleSimpleType
    { 
        $current = $this_SimpleType_0.current; 
        currentNode = currentNode.getParent();
    }

    |
    { 
        currentNode=createCompositeNode(grammarAccess.getTypeAccess().getEntityParserRuleCall_1(), currentNode); 
    }
    this_Entity_1=ruleEntity
    { 
        $current = $this_Entity_1.current; 
        currentNode = currentNode.getParent();
    }

    |
    { 
        currentNode=createCompositeNode(grammarAccess.getTypeAccess().getCampaignTypeParserRuleCall_2(), currentNode); 
    }
    this_CampaignType_2=ruleCampaignType
    { 
        $current = $this_CampaignType_2.current; 
        currentNode = currentNode.getParent();
    }
);





// Entry rule entryRuleSimpleType
entryRuleSimpleType returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getSimpleTypeRule(), currentNode); }
	 iv_ruleSimpleType=ruleSimpleType 
	 { $current=$iv_ruleSimpleType.current; } 
	 EOF 
;

// Rule SimpleType
ruleSimpleType returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
('type' 
    {
        createLeafNode(grammarAccess.getSimpleTypeAccess().getTypeKeyword_0(), null); 
    }
(	
	
	    lv_name_1=	RULE_ID
	{
		createLeafNode(grammarAccess.getSimpleTypeAccess().getNameIDTerminalRuleCall_1_0(), "name"); 
	}
 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getSimpleTypeRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "name", lv_name_1, "ID", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }
	
));





// Entry rule entryRuleCampaignType
entryRuleCampaignType returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getCampaignTypeRule(), currentNode); }
	 iv_ruleCampaignType=ruleCampaignType 
	 { $current=$iv_ruleCampaignType.current; } 
	 EOF 
;

// Rule CampaignType
ruleCampaignType returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
('[campaign]' 
    {
        createLeafNode(grammarAccess.getCampaignTypeAccess().getCampaignKeyword_0(), null); 
    }
(	
	
	    lv_name_1=	RULE_ID
	{
		createLeafNode(grammarAccess.getCampaignTypeAccess().getNameIDTerminalRuleCall_1_0(), "name"); 
	}
 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getCampaignTypeRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "name", lv_name_1, "ID", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }
	
)?'[/campaign]' 
    {
        createLeafNode(grammarAccess.getCampaignTypeAccess().getCampaignKeyword_2(), null); 
    }
);





// Entry rule entryRuleEntity
entryRuleEntity returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getEntityRule(), currentNode); }
	 iv_ruleEntity=ruleEntity 
	 { $current=$iv_ruleEntity.current; } 
	 EOF 
;

// Rule Entity
ruleEntity returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
('entity' 
    {
        createLeafNode(grammarAccess.getEntityAccess().getEntityKeyword_0(), null); 
    }
(	
	
	    lv_name_1=	RULE_ID
	{
		createLeafNode(grammarAccess.getEntityAccess().getNameIDTerminalRuleCall_1_0(), "name"); 
	}
 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getEntityRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "name", lv_name_1, "ID", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }
	
)('extends' 
    {
        createLeafNode(grammarAccess.getEntityAccess().getExtendsKeyword_2_0(), null); 
    }
(	
	
		
		{
			if ($current==null) {
	            $current = factory.create(grammarAccess.getEntityRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
        }
	RULE_ID
	{
		createLeafNode(grammarAccess.getEntityAccess().getExtendsEntityCrossReference_2_1_0(), "extends"); 
	}

		// TODO assign feature to currentNode
	
))?'{' 
    {
        createLeafNode(grammarAccess.getEntityAccess().getLeftCurlyBracketKeyword_3(), null); 
    }
(	
	
	    
	    { 
	        currentNode=createCompositeNode(grammarAccess.getEntityAccess().getPropertiesPropertyParserRuleCall_4_0(), currentNode); 
	    }
	    lv_properties_5=ruleProperty 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getEntityRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode.getParent(), $current);
	        }
	        
	        try {
	       		add($current, "properties", lv_properties_5, "Property", currentNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	        currentNode = currentNode.getParent();
	    }
	
)*'}' 
    {
        createLeafNode(grammarAccess.getEntityAccess().getRightCurlyBracketKeyword_5(), null); 
    }
);





// Entry rule entryRuleProperty
entryRuleProperty returns [EObject current=null] :
	{ currentNode = createCompositeNode(grammarAccess.getPropertyRule(), currentNode); }
	 iv_ruleProperty=ruleProperty 
	 { $current=$iv_ruleProperty.current; } 
	 EOF 
;

// Rule Property
ruleProperty returns [EObject current=null] 
    @init { EObject temp=null; setCurrentLookahead(); resetLookahead(); 
    }
    @after { resetLookahead(); 
    	lastConsumedNode = currentNode;
    }:
('property' 
    {
        createLeafNode(grammarAccess.getPropertyAccess().getPropertyKeyword_0(), null); 
    }
(	
	
	    lv_name_1=	RULE_ID
	{
		createLeafNode(grammarAccess.getPropertyAccess().getNameIDTerminalRuleCall_1_0(), "name"); 
	}
 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getPropertyRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "name", lv_name_1, "ID", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }
	
)':' 
    {
        createLeafNode(grammarAccess.getPropertyAccess().getColonKeyword_2(), null); 
    }
(	
	
		
		{
			if ($current==null) {
	            $current = factory.create(grammarAccess.getPropertyRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
        }
	RULE_ID
	{
		createLeafNode(grammarAccess.getPropertyAccess().getTypeTypeCrossReference_3_0(), "type"); 
	}

		// TODO assign feature to currentNode
	
)(	
	
	    lv_many_4='[]' 
    {
        createLeafNode(grammarAccess.getPropertyAccess().getManyLeftSquareBracketRightSquareBracketKeyword_4_0(), "many"); 
    }

 
	    {
	        if ($current==null) {
	            $current = factory.create(grammarAccess.getPropertyRule().getType().getClassifier());
	            associateNodeWithAstElement(currentNode, $current);
	        }
	        
	        try {
	       		set($current, "many", true, "[]", lastConsumedNode);
	        } catch (ValueConverterException vce) {
				handleValueConverterException(vce);
	        }
	    }
	
)?);





RULE_ID : '^'? ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'_'|'0'..'9')*;

RULE_INT : ('0'..'9')+;

RULE_STRING : ('"' ('\\' ('b'|'t'|'n'|'f'|'r'|'"'|'\''|'\\')|~(('\\'|'"')))* '"'|'\'' ('\\' ('b'|'t'|'n'|'f'|'r'|'"'|'\''|'\\')|~(('\\'|'\'')))* '\'');

RULE_ML_COMMENT : '/*' ( options {greedy=false;} : . )*'*/';

RULE_SL_COMMENT : '//' ~(('\n'|'\r'))* ('\r'? '\n')?;

RULE_WS : (' '|'\t'|'\r'|'\n')+;

RULE_ANY_OTHER : .;


