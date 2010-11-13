package org.wesnoth.tests;

import org.wesnoth.WMLStandaloneSetup;

public class WMLTestsImpl extends WMLTests
{
	@SuppressWarnings("rawtypes")
	@Override
	Class getStandaloneSetupClass() {
		return WMLStandaloneSetup.class;
	}

	//for convenience, define constants for the
	//rule names in your grammar
	//the names of terminal rules will be capitalised
	//and "RULE_" will be appended to the front
	private static final String ID="RULE_ID";
	private static final String INT="RULE_INT";
	private static final String WS="RULE_WS";
	private static final String SL_COMMENT="RULE_SL_COMMENT";

	public void testINT()
	{
		checkTokenisation("1", INT);
	}

	public void testID(){
		checkTokenisation("a", ID);
		checkTokenisation("abc", ID);
		checkTokenisation("abc123", ID);
		checkTokenisation("abc_123", ID);
	}
	public void testSLCOMMENT(){
		checkTokenisation("#comment", SL_COMMENT);
		checkTokenisation("#comment\n", SL_COMMENT);
		checkTokenisation("# comment \t\t comment\r\n", SL_COMMENT);
	}

	public void testTokenSequences(){
		showTokenisation("amount=+$random\n");
		checkParsing("amount=+$random", "WMLKey");

		checkTokenisation("123 abc", ID, WS, ID);
		checkTokenisation("123 \t#comment\n abc", ID, WS, SL_COMMENT,WS,ID);
		//note that no white space is necessary!
		checkTokenisation("123abc", ID);
	}

}