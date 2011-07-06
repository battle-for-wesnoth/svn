/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.builder;

import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.wesnoth.Constants;
import org.wesnoth.Logger;
import org.wesnoth.Messages;
import org.wesnoth.installs.WesnothInstallsUtils;
import org.wesnoth.preferences.Preferences;
import org.wesnoth.preferences.Preferences.Paths;
import org.wesnoth.preprocessor.PreprocessorUtils;
import org.wesnoth.projects.ProjectCache;
import org.wesnoth.projects.ProjectUtils;
import org.wesnoth.utils.AntUtils;
import org.wesnoth.utils.ExternalToolInvoker;
import org.wesnoth.utils.ResourceUtils;
import org.wesnoth.utils.StringUtils;
import org.wesnoth.utils.WMLSaxHandler;
import org.wesnoth.utils.WMLTools;
import org.wesnoth.utils.WorkspaceUtils;
import org.wesnoth.wml.core.ConfigFile;

public class WesnothProjectBuilder extends IncrementalProjectBuilder
{
    private ProjectCache projectCache_;

	@SuppressWarnings("rawtypes")
	@Override
	protected IProject[] build(int kind, Map args, IProgressMonitor monitor)
			throws CoreException
	{
	    if ( WesnothInstallsUtils.setupInstallForResource( getProject() ) == false )
	        return null;

	    if ( projectCache_ == null )
	        projectCache_ = ProjectUtils.getCacheForProject( getProject() );

	    Logger.getInstance().log("building..."); //$NON-NLS-1$
		monitor.beginTask(String.format(Messages.WesnothProjectBuilder_1, getProject().getName()), 100);

        String installName = WesnothInstallsUtils.getInstallNameForResource( getProject() );
		Paths paths = Preferences.getPaths( installName );

		monitor.subTask(Messages.WesnothProjectBuilder_3);
		if ( WorkspaceUtils.checkPathsAreSet( installName, true ) == false )
			return null;
		monitor.worked(5);

		monitor.subTask( "Creating the project tree ..." );
        projectCache_.getDependencyTree( ).createDependencyTree( false );
        monitor.worked( 10 );

		// create the temporary directory used by the plugin if not created
		monitor.subTask(Messages.WesnothProjectBuilder_6);
		WorkspaceUtils.getTemporaryFolder();
		monitor.worked(2);

		if ( runAntJob(paths, monitor ) == false )
		    return null;

		boolean readDefines = false;

		if (kind == FULL_BUILD)
			readDefines = fullBuild(monitor);
		else
		{
			IResourceDelta delta = getDelta(getProject());
			if (delta == null)
				readDefines = fullBuild(monitor);
			else
				readDefines = incrementalBuild(delta, monitor);
		}

		if (readDefines)
		{
			// we read the defines at the end of the build
			// to speed up things (and only if we had any .cfg files processed)
		    projectCache_.readDefines( true );
			projectCache_.saveCache( );

			monitor.worked(10);
		}
		monitor.done();
		return null;
	}

	/**
     * Does a full build on this project
     * @param monitor The monitor used to signal progress
     * @return True if there were config files processed
     * @throws CoreException
     */
    protected boolean fullBuild(final IProgressMonitor monitor) throws CoreException
    {
        projectCache_.getDependencyTree( ).createDependencyTree( true );

        boolean foundCfg = false;

        ProjectDependencyNode node = null, leaf = null;

        node = projectCache_.getDependencyTree( ).getNode(
                DependencyTreeBuilder.ROOT_NODE_KEY );

        if ( node != null ) {

            // going downwards the tree
            do {
                leaf = node;

                // going right the current branch
                do {
                    // process the leaf
                    checkResource( node.getFile( ), monitor, false );

                    leaf = leaf.getNext( );
                }while ( leaf != null );


                node = node.getSon( );
            } while ( node != null );
        }

        return foundCfg;
    }

    /**
     * Does an incremental build on this project
     * @param delta The delta which contains the project modifications
     * @param monitor The monitor used to signal progress
     * @return True if there were config files processed
     * @throws CoreException
     */
    protected boolean incrementalBuild(IResourceDelta delta, IProgressMonitor monitor)
            throws CoreException
    {
        // the visitor does the work.
        boolean foundCfg = false;

        return foundCfg;
    }

    /**
     * Runs the ant job that copies the project in user add-ons directory
     * @param paths The paths instance which contains the paths to wesnoth
     * utilities
     * @param monitor The monitor used to signal progress
     * @return true or false whether the job completed successfully
     */
	private boolean runAntJob( Paths paths, IProgressMonitor monitor )
	{
        // check for 'build.xml' existance
        if (new File(getProject().getLocation().toOSString() + "/build.xml").exists() == true) //$NON-NLS-1$
        {
            // run the ant job to copy the whole project
            // in the user add-ons directory (incremental)
            monitor.subTask(Messages.WesnothProjectBuilder_8);
            Map<String, String> properties = new HashMap<String, String>();
            properties.put("wesnoth.user.dir", paths.getUserDir( )); //$NON-NLS-1$
            Logger.getInstance().log("Ant result:"); //$NON-NLS-1$

            String result = AntUtils.runAnt(
                    getProject().getLocation().toOSString() + "/build.xml", //$NON-NLS-1$
                    properties, true);
            Logger.getInstance().log(result);
            monitor.worked(10);

            if (result == null)
            {
                Logger.getInstance().log("error running the ant job", //$NON-NLS-1$
                        Messages.WesnothProjectBuilder_13);
                return false;
            }
        }

        monitor.worked(2);
        return true;
	}

	protected void handleRemovedResource(IResource resource)
	{
		if ( ResourceUtils.isConfigFile( resource ) )
		{
			projectCache_.getConfigs().remove(resource.getName());
		}
	}

	protected boolean checkResource(IResource resource, IProgressMonitor monitor, boolean handleMainCfg)
	{
		monitor.worked(5);
		if (resource.exists() == false ||
			monitor.isCanceled() ||
			isResourceIgnored(resource))
			return false;

		// config files
		if ( ResourceUtils.isConfigFile( resource ) )
		{
			boolean isMainCfg = resource.getName().equals("_main.cfg"); //$NON-NLS-1$
			if (handleMainCfg == false && isMainCfg == true)
				return true;

			Logger.getInstance().log(""); //$NON-NLS-1$
			try
			{
				IFile file = (IFile) resource;

				monitor.subTask(String.format(Messages.WesnothProjectBuilder_19 ,file.getName()));

				List<String> defines = new ArrayList<String>();
				// for non-main cfg file skip core as we already parsed
				// that when preprocessed main
				if (isMainCfg == false)
					defines.add("SKIP_CORE"); //$NON-NLS-1$

				// we use a single _MACROS_.cfg file for each project
				PreprocessorUtils.getInstance().preprocessFile(file,
						PreprocessorUtils.getInstance().getDefinesLocation(file), defines);
				monitor.worked(5);

				monitor.subTask(Messages.WesnothProjectBuilder_22);

				WMLSaxHandler handler =  (WMLSaxHandler) ResourceUtils.
					getWMLSAXHandlerFromResource(
						PreprocessorUtils.getInstance().getPreprocessedFilePath(file, false, false).toString(),
						new WMLSaxHandler(file.getLocation().toOSString()));

				if (handler != null)
				{
					ConfigFile cfg = handler.getConfigFile();
					projectCache_.getConfigs().put(file.getName(), cfg);
					if (cfg.IsScenario)
					{
						if ( StringUtils.isNullOrEmpty( cfg.ScenarioId ) )
						{
							Logger.getInstance().log("added scenarioId [" + cfg.ScenarioId + //$NON-NLS-1$
									"] for file: " + file.getName()); //$NON-NLS-1$
						}
						else
						{
						    projectCache_.getConfigs().remove(file.getName());
						}
					}
				}
				monitor.worked(10);

			} catch (Exception e)
			{
				Logger.getInstance().logException(e);
			}
		}
		return true;
	}

	@SuppressWarnings( "unused" )
    private void runWMLLint( IProgressMonitor monitor, IFile file )
	{
	    monitor.subTask( String.format( "Running WMLlint on file %s ...", file.getName( ) ) );
	    ExternalToolInvoker tool = WMLTools.runWMLLint(file.getLocation().toOSString(), false, false);
	    tool.waitForTool();

	    try {
            file.deleteMarkers(Constants.MARKER_WMLLINT, false, IResource.DEPTH_INFINITE);
        }
        catch ( CoreException e ) {
            Logger.getInstance( ).logException( e );
        }

        String[] output = StringUtils.getLines(tool.getErrorContent());
        for(String line : output)
            WMLTools.parseAndAddMarkers( line, Constants.MARKER_WMLLINT );

        monitor.worked(20);
	}

	/**
	 * Run the wmlscope for the specified file
	 * @param monitor
	 * @param file
	 * @throws CoreException
	 */
	@SuppressWarnings( "unused" )
    private void runWMLScope( IProgressMonitor monitor, IFile file )
	{
        monitor.subTask( String.format( "Running WMLScope on file %s ...", file.getName( ) ) );
        ExternalToolInvoker tool = WMLTools.runWMLScope( file.getLocation().toOSString(), false );
        tool.waitForTool();

        try {
            file.deleteMarkers(Constants.MARKER_WMLSCOPE, false, IResource.DEPTH_INFINITE);
        }
        catch ( CoreException e ) {
            Logger.getInstance( ).logException( e );
        }

        String[] output = StringUtils.getLines(tool.getErrorContent());
        for(String line : output)
            WMLTools.parseAndAddMarkers( line, Constants.MARKER_WMLSCOPE );
        monitor.worked(20);
	}

	/**
	 * Returns true if the specified resource should be skipped by the builder
	 * @param res
	 * @return
	 */
	private boolean isResourceIgnored(IResource res)
	{
		if (ProjectUtils.getPropertiesForProject(getProject()) == null)
			return false;

		String[] ignored = ProjectUtils.getPropertiesForProject(getProject()).getArray("ignored"); //$NON-NLS-1$
		if (ignored == null)
			return false;

		for (String path : ignored)
		{
			if (path.isEmpty())
				continue;

			if (StringUtils.normalizePath(WorkspaceUtils.getPathRelativeToUserDir(res))
					.contains(StringUtils.normalizePath(path)))
				return true;
		}
		return false;
	}


	class ResourceDeltaVisitor implements IResourceDeltaVisitor
	{
		private IProgressMonitor monitor_;

		public ResourceDeltaVisitor(IProgressMonitor monitor) {
			monitor_ = monitor;
		}

		@Override
		public boolean visit(IResourceDelta delta) throws CoreException
		{
			IResource resource = delta.getResource();
			boolean foundCfg = false;
			IResourceDelta[] affected = delta.getAffectedChildren();

			for(IResourceDelta tmp : affected)
			{
				if ( ResourceUtils.isConfigFile( tmp.getResource( ) ) )
				{
					foundCfg = true;
					break;
				}
			}

			boolean visitChildren = false;
			switch (delta.getKind())
			{
				case IResourceDelta.ADDED:
					// handle added resource
					visitChildren = checkResource(resource, monitor_, false);
					break;
				case IResourceDelta.REMOVED:
					// handle removed resource
					handleRemovedResource(resource);
					visitChildren = true;
					break;
				case IResourceDelta.CHANGED:
					// handle changed resource
					visitChildren = checkResource(resource, monitor_, false);
					break;
			}

			if (foundCfg && resource instanceof IContainer)
			{
				// preprocess _main.cfg before all
				checkResource(((IContainer) resource).getFile(new Path("_main.cfg")), //$NON-NLS-1$
						monitor_, true);
			}

			// return true to continue visiting children.
			return visitChildren;
		}
	}

	/**
	 * This is a WML files comparator, based on the WML parsing rules.
	 *
	 * @see http://wiki.wesnoth.org/PreprocessorRef
	 */
	public static class WMLFilesComparator implements Comparator<IResource> {

        @Override
        public int compare( IResource o1, IResource o2 )
        {
            String name1 = o1.getName( );
            String name2 = o2.getName( );

            // _initial.cfg is always the "lowest"
            if ( name1.equals( "_initial.cfg" ) && !( name2.equals( "_initial.cfg" ) ) ) //$NON-NLS-1$ //$NON-NLS-2$
                return -1;

            if ( name2.equals( "_initial.cfg" ) && !( name1.equals( "_initial.cfg" ) ) ) //$NON-NLS-1$ //$NON-NLS-2$
                return 1;

            // _final.cfg is always the "highest"
            if ( name1.equals( "_final.cfg" ) && !( name2.equals( "_final.cfg" ) ) ) //$NON-NLS-1$ //$NON-NLS-2$
                return 1;

            if ( name2.equals( "_final.cfg" ) && !( name1.equals( "_final.cfg" ) ) ) //$NON-NLS-1$ //$NON-NLS-2$
                return -1;

            return name1.compareTo( o2.getName( ) );
        }
    }
}
