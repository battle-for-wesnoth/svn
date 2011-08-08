/*******************************************************************************
 * Copyright (c) 2011 by Timotei Dolean <timotei21@gmail.com>
 * 
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.wml;

import com.google.common.base.Preconditions;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;

import org.eclipse.core.resources.IFile;
import org.eclipse.emf.common.util.TreeIterator;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.eclipse.xtext.parser.IParseResult;
import org.eclipse.xtext.parser.IParser;

import org.wesnoth.Logger;
import org.wesnoth.projects.ProjectCache;
import org.wesnoth.utils.ResourceUtils;
import org.wesnoth.utils.WMLUtils;
import org.wesnoth.wml.WMLVariable.Scope;

/**
 * A simple WML Parser that parses a xtext WML Config file resource
 */
public class SimpleWMLParser
{
    protected WMLConfig    config_;
    protected WMLRoot      root_;
    protected ProjectCache projectCache_;
    protected int          dependencyIndex_;

    /**
     * Creates a new parser for the specified file
     * 
     * @param file
     *        The file which to parse
     */
    public SimpleWMLParser( IFile file )
    {
        this( file,
                new WMLConfig( file.getProjectRelativePath( ).toString( ) ),
                null );
    }

    /**
     * Creates a new parser and fills the specified config file
     * 
     * @param file
     *        The file which to parse
     * @param config
     *        The config to fill
     * @param projCache
     *        The project cache (can be null) on which to reflect
     *        the parsed data
     */
    public SimpleWMLParser( IFile file, WMLConfig config, ProjectCache projCache )
    {
        config_ = Preconditions.checkNotNull( config );
        root_ = WMLUtils.getWMLRoot( file );
        projectCache_ = projCache;

        dependencyIndex_ = ResourceUtils.getDependencyIndex( file );
    }

    /**
     * Creates a new parser that can be called on files outside the workspace
     */
    public SimpleWMLParser( File file, IParser parser )
            throws FileNotFoundException
    {
        projectCache_ = null;

        IParseResult result = parser.parse( new FileReader( file ) );
        root_ = ( WMLRoot ) result.getRootASTElement( );

        config_ = new WMLConfig( file.getAbsolutePath( ) );
    }

    /**
     * Parses the config. The resulted config will be available in
     * {@link #getParsedConfig()}
     */
    public void parse( )
    {
        TreeIterator< EObject > itor = root_.eAllContents( );
        WMLTag currentTag = null;
        String currentTagName = "";

        // clear tags
        config_.getWMLTags( ).clear( );
        config_.getEvents( ).clear( );

        // nothing to parse!
        if( ! itor.hasNext( ) ) {
            return;
        }

        String currentFileLocation = root_.eResource( ).getURI( )
                .toFileString( );
        if( currentFileLocation == null ) {
            currentFileLocation = root_.eResource( ).getURI( )
                    .toPlatformString( true );
        }

        while( itor.hasNext( ) ) {
            EObject object = itor.next( );

            if( object instanceof WMLTag ) {
                currentTag = ( WMLTag ) object;
                currentTagName = currentTag.getName( );

                if( currentTagName.equals( "scenario" ) ) {
                    config_.IsScenario = true;
                }
                else if( currentTagName.equals( "campaign" ) ) {
                    config_.IsCampaign = true;
                }
            }
            else if( object instanceof WMLKey ) {
                if( currentTag != null ) {
                    WMLKey key = ( WMLKey ) object;
                    String keyName = key.getName( );

                    if( keyName.equals( "id" ) ) {
                        if( currentTagName.equals( "scenario" ) ) {
                            config_.ScenarioId = WMLUtils.getKeyValue( key
                                    .getValues( ) );
                        }
                        else if( currentTagName.equals( "campaign" ) ) {
                            config_.CampaignId = WMLUtils.getKeyValue( key
                                    .getValues( ) );
                        }
                    }
                    else if( keyName.equals( "name" ) ) {
                        if( currentTagName.equals( "set_variable" )
                                || currentTagName.equals( "set_variables" ) ) {
                            handleSetVariable( object );
                        }
                        else if( currentTagName.equals( "clear_variable" )
                                || currentTagName.equals( "clear_variables" ) ) {
                            handleUnsetVariable( object );
                        }
                        else if( currentTagName.equals( "event" ) ) {
                            String eventName = key.getValue( );

                            if( eventName.charAt( 0 ) == '"' ) {
                                eventName = eventName.substring( 1 );
                            }

                            if( eventName.charAt( eventName.length( ) - 1 ) == '"' ) {
                                eventName = eventName.substring( 0,
                                        eventName.length( ) - 1 );
                            }

                            config_.getEvents( ).add( eventName );
                        }
                    }
                }
            }
            else if( object instanceof WMLMacroCall ) {
                WMLMacroCall macroCall = ( WMLMacroCall ) object;
                String macroCallName = macroCall.getName( );
                if( macroCallName.equals( "VARIABLE" ) ) {
                    handleSetVariable( object );
                }
                else if( macroCallName.equals( "CLEAR_VARIABLE" ) ) {
                    handleUnsetVariable( object );
                }
            }
            else if( object instanceof WMLLuaCode ) {
                SimpleLuaParser luaParser = new SimpleLuaParser(
                        currentFileLocation,
                        ( ( WMLLuaCode ) object ).getValue( ) );
                luaParser.parse( );

                config_.getWMLTags( ).putAll( luaParser.getTags( ) );
            }
        }
    }

    protected String getVariableNameByContext( EObject context )
    {
        String variableName = null;

        if( context instanceof WMLKey ) {
            variableName = WMLUtils.getKeyValue( ( ( WMLKey ) context )
                    .getValues( ) );
        }
        else if( context instanceof WMLMacroCall ) {
            WMLMacroCall macro = ( WMLMacroCall ) context;
            if( macro.getParameters( ).size( ) > 0 ) {
                variableName = WMLUtils.toString( macro.getParameters( )
                        .get( 0 ) );
            }
        }

        return variableName;
    }

    protected void handleSetVariable( EObject context )
    {
        if( projectCache_ == null ) {
            return;
        }

        String variableName = getVariableNameByContext( context );

        if( variableName == null ) {
            Logger.getInstance( ).logWarn(
                    "setVariable: couldn't get variable name from context: "
                            + context );
        }

        WMLVariable variable = projectCache_.getVariables( ).get( variableName );
        if( variable == null ) {
            variable = new WMLVariable( variableName );
            projectCache_.getVariables( ).put( variableName, variable );
        }

        int nodeOffset = NodeModelUtils.getNode( context ).getTotalOffset( );
        for( Scope scope: variable.getScopes( ) ) {
            if( scope.contains( dependencyIndex_, nodeOffset ) ) {
                return; // nothing to do
            }
        }

        // couldn't find any scope. Add a new one then.
        variable.getScopes( ).add( new Scope( dependencyIndex_, nodeOffset ) );
    }

    protected void handleUnsetVariable( EObject context )
    {
        if( projectCache_ == null ) {
            return;
        }

        String variableName = getVariableNameByContext( context );
        if( variableName == null ) {
            Logger.getInstance( ).logWarn(
                    "unsetVariable: couldn't get variable name from context: "
                            + context );
        }

        WMLVariable variable = projectCache_.getVariables( ).get( variableName );
        if( variable == null ) {
            return;
        }

        int nodeOffset = NodeModelUtils.getNode( context ).getTotalOffset( );

        // get the first containing scope, and modify its end index/offset

        for( Scope scope: variable.getScopes( ) ) {
            if( scope.contains( dependencyIndex_, nodeOffset ) ) {

                scope.EndIndex = dependencyIndex_;
                scope.EndOffset = nodeOffset;

                return;
            }
        }
    }

    /**
     * Returns the parsed WMLConfig
     * 
     * @return Returns the parsed WMLConfig
     */
    public WMLConfig getParsedConfig( )
    {
        return config_;
    }
}
