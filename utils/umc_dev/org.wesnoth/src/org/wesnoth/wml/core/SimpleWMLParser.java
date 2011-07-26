/*******************************************************************************
 * Copyright (c) 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.wml.core;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.resources.IFile;
import org.eclipse.emf.common.util.TreeIterator;
import org.eclipse.emf.ecore.EObject;
import org.eclipse.xtext.nodemodel.util.NodeModelUtils;
import org.wesnoth.Logger;
import org.wesnoth.projects.ProjectCache;
import org.wesnoth.schema.Tag;
import org.wesnoth.utils.ResourceUtils;
import org.wesnoth.utils.WMLUtils;
import org.wesnoth.wml.WMLKey;
import org.wesnoth.wml.WMLLuaCode;
import org.wesnoth.wml.WMLMacroCall;
import org.wesnoth.wml.WMLRoot;
import org.wesnoth.wml.WMLTag;
import org.wesnoth.wml.core.WMLVariable.Scope;

import com.google.common.base.Preconditions;

/**
 * A simple WML Parser that parses a xtext WML Config file resource
 */
public class SimpleWMLParser
{
    protected WMLConfig config_;
    protected IFile file_;
    protected ProjectCache projectCache_;
    protected int dependencyIndex_;
    protected List<Tag> tags_;

    /**
     * Creates a new parser for the specified file
     *
     * @param file The file which to parse
     */
    public SimpleWMLParser( IFile file )
    {
        this( file, new WMLConfig( file.getProjectRelativePath( ).toString( ) ), null );
    }

    /**
     * Creates a new parser and fills the specified config file
     *
     * @param file The file which to parse
     * @param config The config to fill
     * @param projCache The project cache (can be null) on which to reflect
     * the parsed data
     */
    public SimpleWMLParser( IFile file, WMLConfig config, ProjectCache projCache )
    {
        config_ = Preconditions.checkNotNull( config );
        file_ = file;
        projectCache_ = projCache;

        tags_ = new ArrayList<Tag>();

        dependencyIndex_ = ResourceUtils.getDependencyIndex( file );
    }

    /**
     * Parses the config. The resulted config will be available in {@link #getParsedConfig()}
     */
    public void parse( )
    {
        WMLRoot root = ResourceUtils.getWMLRoot( file_ );
        TreeIterator<EObject> itor = root.eAllContents( );
        WMLTag currentTag = null;
        String currentTagName = "";

        while ( itor.hasNext( ) ) {
            EObject object = itor.next( );

            if ( object instanceof WMLTag ) {
                currentTag = ( WMLTag ) object;
                currentTagName = currentTag.getName( );

                if ( currentTagName.equals( "scenario" ) )
                    config_.IsScenario = true;
                else if ( currentTagName.equals( "campaign" ) )
                    config_.IsCampaign = true;
            }
            else if ( object instanceof WMLKey ) {
                if ( currentTag != null ) {
                    WMLKey key = ( WMLKey ) object;
                    String keyName = key.getName( );

                    if ( keyName.equals( "id" ) ) {
                        if ( currentTagName.equals( "scenario" ) )
                            config_.ScenarioId = WMLUtils.getKeyValue( key.getValue( ) );
                        else if ( currentTagName.equals( "campaign" ) )
                            config_.CampaignId = WMLUtils.getKeyValue( key.getValue( ) );
                    } else if ( keyName.equals( "name" ) ) {
                        if ( currentTagName.equals( "set_variable" ) ||
                             currentTagName.equals( "set_variables" ) ) {
                            handleSetVariable( object );
                        } else if ( currentTagName.equals( "clear_variable" ) ||
                                    currentTagName.equals( "clear_variables" ) ) {
                            handleUnsetVariable( object );
                        }
                    }
                }
            }
            else if ( object instanceof WMLMacroCall ) {
                WMLMacroCall macroCall = ( WMLMacroCall ) object;
                String macroCallName = macroCall.getName( );
                if ( macroCallName.equals( "VARIABLE" ) ) {
                    handleSetVariable( object );
                } else if ( macroCallName.equals( "CLEAR_VARIABLE" ) ) {
                    handleUnsetVariable( object );
                }
            }
            else if ( object instanceof WMLLuaCode ) {
                SimpleLuaParser luaParser = new SimpleLuaParser(
                        ( ( WMLLuaCode ) object ).getValue( ) );
                luaParser.parse( );
                tags_.addAll( luaParser.getTags( ) );
            }
        }
        //TODO: parse custom events

        System.out.println( "parsed config: " + config_ );
    }

    protected String getVariableNameByContext( EObject context )
    {
        String variableName = null;

        if ( context instanceof WMLKey ) {
            variableName =  WMLUtils.getKeyValue( ( ( WMLKey ) context ).getValue( ) ) ;
        } else if ( context instanceof WMLMacroCall ) {
            WMLMacroCall macro = ( WMLMacroCall ) context;
            if ( macro.getParameters( ).size( ) > 0 ) {
                variableName = WMLUtils.toString( macro.getParameters( ).get( 0 ) ) ;
            }
        }

        return variableName;
    }


    protected void handleSetVariable( EObject context )
    {
        if ( projectCache_ == null )
            return;

        String variableName = getVariableNameByContext( context );

        if ( variableName == null ) {
            Logger.getInstance( ).logWarn(
                    "setVariable: couldn't get variable name from context: " + context );
        }

        WMLVariable variable = projectCache_.getVariables( ).get( variableName );
        if ( variable == null ) {
            variable = new WMLVariable( variableName );
            projectCache_.getVariables( ).put( variableName, variable );
        }

        int nodeOffset = NodeModelUtils.getNode( context ).getTotalOffset( );
        for ( Scope scope : variable.getScopes( ) ) {
            if ( scope.contains( dependencyIndex_, nodeOffset ) )
                return; // nothing to do
        }

        // couldn't find any scope. Add a new one then.
        variable.getScopes( ).add( new Scope( dependencyIndex_, nodeOffset ) );
        System.out.println( "new scope for variable:" + variable );
    }

    protected void handleUnsetVariable( EObject context )
    {
        if ( projectCache_ == null )
            return;

        String variableName = getVariableNameByContext( context );
        if ( variableName == null ) {
            Logger.getInstance( ).logWarn(
                    "unsetVariable: couldn't get variable name from context: " + context );
        }

        WMLVariable variable = projectCache_.getVariables( ).get( variableName );
        if ( variable == null )
            return;

        int nodeOffset = NodeModelUtils.getNode( context ).getTotalOffset( );

        // get the first containing scope, and modify its end index/offset

        for ( Scope scope : variable.getScopes( ) ) {
            if ( scope.contains( dependencyIndex_, nodeOffset ) ) {

                scope.EndIndex = dependencyIndex_;
                scope.EndOffset = nodeOffset;

                System.out.println( "new end for variable:" + variable );
                return;
            }
        }
    }

    /**
     * Returns the parsed tags
     * @return A list of tags
     */
    public List<Tag> getParsedTags()
    {
        return tags_;
    }

    /**
     * Returns the parsed WMLConfig
     * @return Returns the parsed WMLConfig
     */
    public WMLConfig getParsedConfig()
    {
        return config_;
    }
}
