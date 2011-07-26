package org.wesnoth.tests;

import java.io.StringReader;
import java.util.List;

import org.antlr.runtime.ANTLRStringStream;
import org.antlr.runtime.CharStream;
import org.antlr.runtime.Token;
import org.eclipse.xtext.ParserRule;
import org.eclipse.xtext.junit.AbstractXtextTests;
import org.eclipse.xtext.nodemodel.INode;
import org.eclipse.xtext.parser.IParseResult;
import org.eclipse.xtext.parser.IParser;
import org.eclipse.xtext.parser.antlr.ITokenDefProvider;
import org.eclipse.xtext.parser.antlr.Lexer;
import org.eclipse.xtext.parser.antlr.XtextTokenStream;
import org.wesnoth.WMLStandaloneSetup;
import org.wesnoth.services.WMLGrammarAccess;

@SuppressWarnings( "all" )
public abstract class WMLTests extends AbstractXtextTests
{
    protected WMLGrammarAccess grammar;
    private Lexer lexer;
    private ITokenDefProvider tokenDefProvider;
    private IParser parser;

    protected Lexer getLexer()
    {
        return lexer;
    }

    protected ITokenDefProvider getTokenDefProvider()
    {
        return tokenDefProvider;
    }

    @Override
    protected IParser getParser()
    {
        return parser;
    }

    Class getStandaloneSetupClass() {
        return WMLStandaloneSetup.class;
    }

    @Override
    protected void setUp() throws Exception
    {
        super.setUp( );
        with( getStandaloneSetupClass( ) );
        lexer = get( Lexer.class );
        tokenDefProvider = get( ITokenDefProvider.class );
        parser = get( IParser.class );
        grammar = ( WMLGrammarAccess ) getGrammarAccess( );
    }

    /**
     * return the list of tokens created by the lexer from the given input
     */
    protected List<Token> getTokens( String input )
    {
        CharStream stream = new ANTLRStringStream( input );
        getLexer( ).setCharStream( stream );
        XtextTokenStream tokenStream = new XtextTokenStream( getLexer( ), getTokenDefProvider( ) );
        List<Token> tokens = tokenStream.getTokens( );
        return tokens;
    }

    /**
     * return the name of the terminal rule for a given token
     */
    protected String getTokenType( Token token )
    {
        return getTokenDefProvider( ).getTokenDefMap( ).get( token.getType( ) );
    }

    /**
     * check whether an input is chopped into a list of expected token types
     */
    protected void checkTokenisation( String input, String... expectedTokenTypes )
    {
        List<Token> tokens = getTokens( input );
        assertEquals( input, expectedTokenTypes.length, tokens.size( ) );
        for ( int i = 0; i < tokens.size( ); i++ ) {
            Token token = tokens.get( i );
            assertEquals( input, expectedTokenTypes[i], getTokenType( token ) );
        }
    }

    protected void showTokenisation( String input )
    {
        List<Token> tokens = getTokens( input );
        for ( int i = 0; i < tokens.size( ); i++ ) {
            Token token = tokens.get( i );
            System.out.println( getTokenType( token ) );
        }
    }

    /**
     * check that an input is not tokenised using a particular terminal rule
     */
    protected void failTokenisation( String input, String unExpectedTokenType )
    {
        List<Token> tokens = getTokens( input );
        assertEquals( input, 1, tokens.size( ) );
        Token token = tokens.get( 0 );
        assertNotSame( input, unExpectedTokenType, getTokenType( token ) );
    }

    /**
     * return the parse result for an input given a specific entry rule of the
     * grammar
     */
    protected IParseResult getParseResult( String input, ParserRule entryRule )
    {
        return getParser( ).parse( entryRule, new StringReader( input ) );
    }

    /**
     * check that the input can be successfully parsed given a specific entry
     * rule of the grammar
     */
    protected void checkParsing( String input, ParserRule entryRule )
    {
        IParseResult la = getParseResult( input, entryRule );
        for ( INode node : la.getSyntaxErrors( ) ) {
            System.out.println( node.getSyntaxErrorMessage( ) );
        }
        assertEquals( input, false, la.hasSyntaxErrors( ) );
    }

    /**
     * check that the input cannot be successfully parsed given a specific entry
     * rule of the grammar
     */
    protected void failParsing( String input, ParserRule entryRule )
    {
        IParseResult la = getParseResult( input, entryRule );
        assertNotSame( input, false, la.hasSyntaxErrors( ) );
    }

    /**
     * check that input is treated as a keyword by the grammar
     */
    protected void checkKeyword( String input )
    {
        // the rule name for a keyword is usually
        // the keyword enclosed in single quotes
        String rule = new StringBuilder( "'" ).append( input ).append( "'" ) //$NON-NLS-1$ //$NON-NLS-2$
        .toString( );
        checkTokenisation( input, rule );
    }

    /**
     * check that input is not treated as a keyword by the grammar
     */
    protected void failKeyword( String keyword )
    {
        List<Token> tokens = getTokens( keyword );
        assertEquals( keyword, 1, tokens.size( ) );
        String type = getTokenType( tokens.get( 0 ) );
        assertFalse( keyword, type.charAt( 0 ) == '\'' );
    }
}
