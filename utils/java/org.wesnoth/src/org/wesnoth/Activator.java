/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth;

import java.util.Map.Entry;

import org.eclipse.core.resources.IProject;
import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;
import org.wesnoth.utils.PreprocessorUtils;
import org.wesnoth.utils.ProjectCache;
import org.wesnoth.utils.ProjectUtils;
import org.wesnoth.utils.WorkspaceUtils;


/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin
{
	// The shared instance
	private static Activator	plugin;
	/**
	 * Switch for knowing if we already started checking conditions
	 * This way we prevent a stack overflow occuring
	 */
	public boolean IsCheckingConditions = false;
	/**
	 * Switch for knowing if we already intend to setup the workspace
	 */
	public boolean IsSettingUpWorkspace = false;

	/**
	 * The constructor
	 */
	public Activator() {
	}

	@Override
	public void start(BundleContext context) throws Exception
	{
		super.start(context);
		plugin = this;
		Logger.getInstance().startLogger();
	}

	@Override
	public void stop(BundleContext context) throws Exception
	{
		// save the caches of the projects on disk
		for(Entry<IProject, ProjectCache> cache :
					ProjectUtils.getProjectCaches().entrySet())
		{
			cache.getValue().saveCache();
		}
		PreprocessorUtils.getInstance().saveTimestamps();

		Logger.getInstance().stopLogger();

		plugin = null;
		super.stop(context);
	}

	/**
	 * Returns the shared instance
	 *
	 * @return the shared instance
	 */
	public static Activator getDefault()
	{
		if (plugin.IsCheckingConditions == false &&
			PlatformUI.isWorkbenchRunning())
		{
			if (WorkspaceUtils.checkConditions(false) == false &&
				plugin.IsSettingUpWorkspace == false)
			{
				WorkspaceUtils.setupWorkspace(true);
			}
		}
		return plugin;
	}

	/**
	 * Returns an image descriptor for the image file at the given plug-in
	 * relative path
	 *
	 * @param path the path
	 * @return the image descriptor
	 */
	public static ImageDescriptor getImageDescriptor(String path)
	{
		return imageDescriptorFromPlugin(Constants.PLUGIN_ID, path);
	}

	/**
	 * Returns the plugin's shell
	 *
	 * @return
	 */
	public static Shell getShell()
	{
		return plugin.getWorkbench().getDisplay().getActiveShell();
	}
}
