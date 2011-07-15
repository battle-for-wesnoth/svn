/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.projects;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IProject;
import org.eclipse.jface.dialogs.DialogSettings;
import org.eclipse.jface.dialogs.IDialogSettings;
import org.wesnoth.Constants;
import org.wesnoth.Logger;
import org.wesnoth.builder.DependencyListBuilder;
import org.wesnoth.preferences.Preferences;
import org.wesnoth.preprocessor.Define;
import org.wesnoth.preprocessor.PreprocessorUtils;
import org.wesnoth.utils.ResourceUtils;
import org.wesnoth.utils.WorkspaceUtils;
import org.wesnoth.wml.core.ConfigFile;
import org.wesnoth.wml.core.Variable;

/**
 * A class that stores some project specific infos
 * for current session.
 * Some of the fields of this cache can be saved to disk
 *  @see ProjectCache#saveCache()
 */
public class ProjectCache
{
    private long propertiesTimestamp_;
    private long definesTimestamp_;

    private DialogSettings properties_;

    private File wesnothFile_;
    private File definesFile_;
    private File treeCacheFile_;

    private Map< String, ConfigFile > configFiles_;
    private Map< String, Define > defines_;
    private DependencyListBuilder dependTree_;

    private IProject project_;

    public ProjectCache(IProject project)
    {
        project_ = project;

        configFiles_ = new HashMap<String, ConfigFile>();
        defines_ = new HashMap<String, Define>(0);

        dependTree_ = new DependencyListBuilder( project_ );

        propertiesTimestamp_ = 0;
        properties_ = new DialogSettings("project"); //$NON-NLS-1$

        wesnothFile_ = new File(project.getLocation().toOSString()  +
        "/.wesnoth"); //$NON-NLS-1$

        definesFile_ = new File (PreprocessorUtils.getInstance().getMacrosLocation( project ));
        treeCacheFile_ = new File ( WorkspaceUtils.getProjectTemporaryFolder( project ) + "/_TREE_CACHE_.bin" ); //$NON-NLS-1$

        ResourceUtils.createWesnothFile(wesnothFile_.getAbsolutePath(), false);
        readProperties(true);
        readDefines(true);
    }

    /**
     * Reads the properties from the file only if the
     * file changed.
     * @param force True to skip checking for modifications
     */
    private void readProperties(boolean force)
    {
        if (force == false &&
                wesnothFile_.lastModified() <= propertiesTimestamp_)
            return;

        try
        {
            ResourceUtils.createWesnothFile(wesnothFile_.getAbsolutePath(), false);

            try
            {
                properties_.load( wesnothFile_.getAbsolutePath() );
            }
            catch(ClassCastException ex)
            {
                // backward compatiblity
                // we already have an xml format used by Properties.
                // convert it to DialogSettings
                ResourceUtils.createWesnothFile(wesnothFile_.getAbsolutePath(), true);
                properties_.load( wesnothFile_.getAbsolutePath() );
            }

            if (properties_.getSection("configs") != null) //$NON-NLS-1$
            {
                for(IDialogSettings config : properties_.getSection("configs").getSections()) //$NON-NLS-1$
                {
                    if (config.getName().startsWith("config") == false) //$NON-NLS-1$
                        continue;

                    ConfigFile tmp = new ConfigFile(config.get("filename")); //$NON-NLS-1$
                    tmp.ScenarioId = config.get( "scenario_id" ); //$NON-NLS-1$
                    tmp.CampaignId = config.get( "campaign_id" ); //$NON-NLS-1$

                    for(IDialogSettings variable : config.getSection("variables").getSections()) //$NON-NLS-1$
                    {
                        if (variable.getName().startsWith("var") == false) //$NON-NLS-1$
                            continue;
                        tmp.getVariables().add(
                                new Variable(variable.get("name"), //$NON-NLS-1$
                                        variable.get("location"), //$NON-NLS-1$
                                        variable.getInt("offset"))); //$NON-NLS-1$
                    }
                    configFiles_.put(config.get("filename"), tmp); //$NON-NLS-1$
                }
            }

            // unserialize the tree builder
            if ( treeCacheFile_.exists( ) ) {
                FileInputStream inStream = new FileInputStream( treeCacheFile_ );
                ObjectInputStream deserializer = new ObjectInputStream( inStream );
                dependTree_.deserialize( deserializer );
            }

            propertiesTimestamp_ = wesnothFile_.lastModified();
        }
        catch (Exception e)
        {
            Logger.getInstance().logException(e);
            propertiesTimestamp_ = 0; // force to re-read the file
        }
    }

    /**
     * Gets the properties store for the associated Project.
     * If the store doesn't exist it will be created.
     * This method ensures it will get the latest up-to-date '.wesnoth' file
     * @return
     */
    public DialogSettings getProperties()
    {
        readProperties(false);
        return properties_;
    }

    /**
     * Gets the map with the Configs
     * The key represent the filenames of the files
     * and the value the scenarioId from that file
     * @return
     */
    public Map<String, ConfigFile> getConfigs()
    {
        return configFiles_;
    }

    /**
     * Gets the Config by it's filename.
     * If the Config doesn't exist it will be created
     * @param name
     * @return
     */
    public ConfigFile getConfig(String name)
    {
        ConfigFile config = configFiles_.get( name );
        if ( config == null ){
            config = new ConfigFile( name );
            configFiles_.put( name, config );
        }

        return config;
    }

    /**
     * Saves the cache to disk.
     * Saves:
     * - properties
     * - existing scenarios
     * @return
     */
    public boolean saveCache()
    {
        ResourceUtils.createWesnothFile(wesnothFile_.getAbsolutePath(), false);
        try
        {
            // save config files info
            int configCnt = 0;
            IDialogSettings configsSection = properties_.addNewSection("configs"); //$NON-NLS-1$
            for(ConfigFile config : configFiles_.values())
            {
                IDialogSettings configSection = configsSection.addNewSection("config" + configCnt); //$NON-NLS-1$
                configSection.put("scenario_id", config.ScenarioId); //$NON-NLS-1$
                configSection.put("campaign_id", config.CampaignId); //$NON-NLS-1$
                configSection.put("filename", config.getFilename()); //$NON-NLS-1$

                IDialogSettings variablesSection = configSection.addNewSection("variables"); //$NON-NLS-1$
                int varCnt = 0;
                for(Variable var : config.getVariables())
                {
                    IDialogSettings varSection = variablesSection.addNewSection("var" + varCnt); //$NON-NLS-1$
                    varSection.put("name", var.getName()); //$NON-NLS-1$
                    varSection.put("location", var.getLocation()); //$NON-NLS-1$
                    varSection.put("offset", var.getOffset()); //$NON-NLS-1$

                    ++varCnt;
                }
                ++configCnt;
            }

            // store properties
            properties_.save( wesnothFile_.getAbsolutePath() );
            propertiesTimestamp_ = wesnothFile_.lastModified();

            // save the PDT tree
            ResourceUtils.createNewFile( treeCacheFile_.getAbsolutePath( ) );
            FileOutputStream outStream = new FileOutputStream( treeCacheFile_ );
            ObjectOutputStream serializer = new ObjectOutputStream( outStream );
            serializer.writeObject( dependTree_ );
            serializer.close( );

            return true;
        }
        catch (Exception e)
        {
            Logger.getInstance().logException(e);
            return false;
        }
    }

    /**
     * Reads the defines files for this project
     * @param force skip checking for last modified timestamp
     */
    public void readDefines(boolean force)
    {
        if (force == false &&
                definesFile_.lastModified() <= definesTimestamp_)
            return;
        if (definesFile_.exists() == false)
            return;
        defines_ = Define.readDefines(definesFile_.getAbsolutePath());
        definesTimestamp_ = definesFile_.lastModified( );
    }

    public void setDefines(Map<String, Define> defines)
    {
        defines_ = defines;
    }

    /**
     * Returns the defines associated with this project
     * @return
     */
    public Map<String, Define> getDefines()
    {
        return defines_;
    }

    /**
     * The name of the install used in the project
     */
    public String getInstallName()
    {
        return Preferences.getString( Constants.P_INST_NAME_PREFIX + project_.getName( ) );
    }

    /**
     * Sets the new install used in the project
     * @param newInstallName The new install name
     */
    public void setInstallName( String newInstallName )
    {
        Preferences.getPreferences( ).setValue(
                Constants.P_INST_NAME_PREFIX + project_.getName( ),
                newInstallName );
    }

    /**
     * Returns the current dependency tree builder for this project
     * @return A dependency tree
     */
    public DependencyListBuilder getDependencyList()
    {
        return dependTree_;
    }

    /**
     * Clears all the project cache
     */
    public void clear()
    {
        properties_ = new DialogSettings("project"); //$NON-NLS-1$

        configFiles_.clear( );
        defines_.clear( );
        dependTree_ = new DependencyListBuilder( project_ );

        propertiesTimestamp_ = -1;
        definesTimestamp_ = -1;

        saveCache( );
    }
}
