/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.utils;

import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicInteger;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.WorkspaceJob;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.ui.IEditorReference;
import org.eclipse.ui.console.MessageConsole;
import org.wesnoth.Constants;
import org.wesnoth.Logger;
import org.wesnoth.Messages;
import org.wesnoth.installs.WesnothInstallsUtils;
import org.wesnoth.preferences.Preferences;
import org.wesnoth.preferences.Preferences.Paths;

public class WMLTools
{
	/**
	 * Runs "wmlindent" on the specified resource (directory/file)
	 *
	 * @param resourcePath the full path of the target where "wmlindent" will be runned on
	 * @param stdin the standard input string to feed "wmlindent"
	 * @param dryrun true to run "wmlindent" in dry mode - i.e. no changes in the config file.
	 * @param stdout The array of streams where to output the stdout content
	 * @param stderr The array of streams where to output the stderr content
	 * @return  null if there were errors or an ExternalToolInvoker instance
	 */
	public static ExternalToolInvoker runWMLIndent(String resourcePath, String stdin,
			boolean dryrun, OutputStream[] stdout, OutputStream[] stderr)
	{
        Paths paths = Preferences.getPaths( WesnothInstallsUtils.getInstallNameForResource( resourcePath ) );

        //wmlindent only check first
		if ( !checkWMLTool( paths, Tools.WMLINDENT.toString( ) ) ) //$NON-NLS-1$
			return null;

		File wmllintFile = new File( paths.getWMLToolsDir( ) + "/wmlindent" ); //$NON-NLS-1$
		List<String> arguments = new ArrayList<String>();
		arguments.add(wmllintFile.getAbsolutePath());

		if (resourcePath != null)
		{
			if ( !ResourceUtils.isValidFilePath( resourcePath ) )
				return null;

			if (dryrun || Preferences.getBool(Constants.P_WMLINDENT_DRYRUN) == true)
				arguments.add("--dryrun"); //$NON-NLS-1$

			if (Preferences.getBool(Constants.P_WMLINDENT_VERBOSE))
			{
				arguments.add("-v"); //$NON-NLS-1$
				arguments.add("-v"); //$NON-NLS-1$
			}
			arguments.add(resourcePath);
		}
		return runPythonScript(arguments, stdin, true, true, stdout,stderr);
	}

	/**
	 * Runs a wmlparser on the target resource
	 * @param resourcePath
	 * @return null if there were errors or an ExternalToolInvoker instance
	 */
	public static ExternalToolInvoker runWMLParser2(String resourcePath)
	{
        Paths paths = Preferences.getPaths( WesnothInstallsUtils.getInstallNameForResource( resourcePath ) );

		if ( !ResourceUtils.isValidFilePath( resourcePath ) ||
	         !checkWMLTool( paths, "wesnoth/wmlparser2.py" ) ) //$NON-NLS-1$
			return null;

		File wmlparserFile = new File(paths.getWMLToolsDir( ) + "/wesnoth/wmlparser2.py" ); //$NON-NLS-1$

		List<String> arguments = new ArrayList<String>();

		arguments.add(wmlparserFile.getAbsolutePath());

		// xml output
		arguments.add( "-x" ); //$NON-NLS-1$

		// no preprocess
		arguments.add( "-n" ); //$NON-NLS-1$

		// wesnoth executable's path
		arguments.add( "-w" ); //$NON-NLS-1$
		arguments.add( paths.getWesnothExecutablePath( ) );

		// add user data directory
		arguments.add( "-c"); //$NON-NLS-1$
		arguments.add( paths.getUserDir( ) );

		// add the working data directory
		arguments.add( "-a" ); //$NON-NLS-1$
		arguments.add( paths.getWorkingDir( ) );

		// input file
		arguments.add( "-i" ); //$NON-NLS-1$
		arguments.add( resourcePath );

		return runPythonScript(arguments, null, false, false, null, null);
	}

	/**
	 * Runs "wmllint" on the specified resource (directory/file)
	 *
	 * @param resourcePath the full path of the target where "wmllint" will be runned on
	 * @param dryrun true to run "wmllint" in dry mode - i.e. no changes in the config file.
	 * @return  null if there were errors or an ExternalToolInvoker instance
	 */
	public static ExternalToolInvoker runWMLLint(String resourcePath, boolean dryrun, boolean showProgress)
	{
		return runWMLLint(resourcePath, dryrun, showProgress, new OutputStream[0], new OutputStream[0]);
	}

	/**
	 * Runs "wmllint" on the specified resource (directory/file)
	 *
	 * @param resourcePath the full path of the target where "wmllint" will be runned on
	 * @param dryrun true to run "wmllint" in dry mode - i.e. no changes in the config file.
	 * @param stdout The array of streams where to output the stdout content
	 * @param stderr The array of streams where to output the stderr content
	 */
	public static ExternalToolInvoker runWMLLint(String resourcePath, boolean dryrun,
				boolean showProgress, OutputStream[] stdout, OutputStream[] stderr)
	{
        Paths paths = Preferences.getPaths( WesnothInstallsUtils.getInstallNameForResource( resourcePath ) );

		if ( !ResourceUtils.isValidFilePath( resourcePath ) ||
	         !checkWMLTool( paths, Tools.WMLLINT.toString( ) ) )
			return null;

		File wmllintFile = new File( paths.getWMLToolsDir( ) + "/wmllint" ); //$NON-NLS-1$

		List<String> arguments = new ArrayList<String>();

		arguments.add(wmllintFile.getAbsolutePath());

		int verboseLevel = Preferences.getInt(Constants.P_WMLLINT_VERBOSE_LEVEL);
		for(int i=1; i <= verboseLevel; i++)
			arguments.add("-v"); //$NON-NLS-1$

		if (verboseLevel <= 0 && showProgress)
			arguments.add("--progress"); //$NON-NLS-1$

		if (dryrun || Preferences.getBool(Constants.P_WMLLINT_DRYRUN) == true)
			arguments.add("--dryrun"); //$NON-NLS-1$

		if (Preferences.getBool(Constants.P_WMLLINT_SPELL_CHECK) == false)
			arguments.add("--nospellcheck"); //$NON-NLS-1$

		// add default core directory
		arguments.add( paths.getCoreDir( ) );
		arguments.add( resourcePath );

		return runPythonScript(arguments, null, true, true, stdout,stderr);
	}

	/**
	 * Runs "wmlscope" on the specified resource (directory/file)
	 *
	 * @param resourcePath the full path of the target where "wmlindent" will be runned on
	 * @return  null if there were errors or an ExternalToolInvoker instance
	 */
	public static ExternalToolInvoker runWMLScope(String resourcePath, boolean showProgress)
	{
		return runWMLScope(resourcePath, showProgress, new OutputStream[0], new OutputStream[0]);
	}

	/**
	 * Runs "wmlscope" on the specified resource (directory/file)
	 *
	 * @param resourcePath the full path of the target where "wmlindent" will be runned on
	 * @param stdout The array of streams where to output the stdout content
	 * @param stderr The array of streams where to output the stderr content
	 * @return  null if there were errors or an ExternalToolInvoker instance
	 */
	public static ExternalToolInvoker runWMLScope(String resourcePath, boolean showProgress,
			OutputStream[] stdout, OutputStream[] stderr)
	{
        Paths paths = Preferences.getPaths( WesnothInstallsUtils.getInstallNameForResource( resourcePath ) );

		if ( !ResourceUtils.isValidFilePath( resourcePath ) ||
		     !checkWMLTool( paths, Tools.WMLSCOPE.toString( ) ) )
			return null;

		File wmlscopeFile = new File( paths.getWMLToolsDir( ) + "/wmlscope" ); //$NON-NLS-1$

		List<String> arguments = new ArrayList<String>();

		arguments.add(wmlscopeFile.getAbsolutePath());
		int verboseLevel = Preferences.getInt(Constants.P_WMLSCOPE_VERBOSE_LEVEL);

		if (verboseLevel > 0)
		{
			arguments.add("-w"); //$NON-NLS-1$
			arguments.add(String.valueOf(verboseLevel));
		}
		else if (showProgress)
			arguments.add("--progress"); //$NON-NLS-1$

		if (Preferences.getBool(Constants.P_WMLSCOPE_COLLISIONS) == true)
			arguments.add("--collisions"); //$NON-NLS-1$

		arguments.add( "--unchecked" ); //$NON-NLS-1$
		arguments.add( "--unresolved" ); //$NON-NLS-1$

		// add default core directory
		arguments.add( paths.getCoreDir( ) );
		arguments.add( resourcePath );

		return runPythonScript(arguments, null, true, true, stdout, stderr);
	}

	/**
	 * Runs the specified WMLTools as a workspace job
	 * @param tool The tool to run.
	 * Currently only the following tools are supported: WMLLINT, WMLSCOPE, WMLINDENT
	 * @param targetPath If this is not null if will use the targetpath as the
	 * argument for launching the tool
	 */
	public static void runWMLToolAsWorkspaceJob(final Tools tool, final String targetPath)
	{
		if (tool == Tools.WESNOTH_ADDON_MANAGER)
			return;

		IEditorReference[] editors = WorkspaceUtils.getWorkbenchWindow().getPages()[0].getEditorReferences();

		for (IEditorReference editor : editors)
		{
			if (editor.isDirty())
				editor.getEditor(false).doSave(null);
		}

		if (targetPath == null && WorkspaceUtils.getSelectedFile() != null)
			EditorUtils.openEditor(WorkspaceUtils.getSelectedFile(), false);
		final String toolName = tool.toString();

		WorkspaceJob job = new WorkspaceJob(Messages.WMLTools_28 + toolName) {
			private ExternalToolInvoker toolInvoker;
			private AtomicInteger workReporter = new AtomicInteger();

			@Override
			protected void canceling()
			{
				toolInvoker.kill(true);
				super.canceling();
			}

			@Override
			public IStatus runInWorkspace(final IProgressMonitor monitor)
			{
				try{
					monitor.beginTask(toolName, 1050);
					MessageConsole console = GUIUtils.createConsole(toolName + Messages.WMLTools_29, null, true);
					OutputStream[] stdout = new OutputStream[]{ console.newMessageStream() };
					OutputStream[] stderr = new OutputStream[]{ console.newMessageStream() };

					String location;
					IResource resource = null;

					IFile selFile = WorkspaceUtils.getSelectedFile();
					if (targetPath != null)
						location = targetPath;
					else
					{
						if (selFile != null)
						{
							location = selFile.getLocation().toOSString();
							resource = selFile;
						}
						else
						{
							resource = WorkspaceUtils.getSelectedContainer();
							location = resource.getLocation().toOSString();
						}
					}

					switch(tool)
					{
						case WMLINDENT:
							if (selFile != null && targetPath == null)
							{
								String stdin = EditorUtils.getEditorDocument().get();
								// don't output to stdout as we will put that in the editor
								toolInvoker = WMLTools.runWMLIndent(null, stdin, false,
										null, stdout);
							}
							else
								toolInvoker = WMLTools.runWMLIndent(location, null, false,
										stdout, stderr);
							break;
						case WMLLINT:
							toolInvoker = WMLTools.runWMLLint(location, true, false, stdout, stderr);
							break;
						case WMLSCOPE:
							toolInvoker = WMLTools.runWMLScope(location, false, stdout, stderr);
							break;
					}
					monitor.worked(50);
					// need to fill up to '1000' worked
					// we will add 1 for each 2 lines of output (for each file)
					Thread stdoutWatcher = new Thread(new Runnable(){
						@Override
						public void run()
						{
							int nr;
							while (toolInvoker.readOutputLine() != null)
							{
								nr = workReporter.incrementAndGet();
								if (nr % 2 == 0)
									synchronized (monitor)
									{
										monitor.worked(1);
									}
							}
						}
					});
					Thread stderrWatcher = new Thread(new Runnable(){
						@Override
						public void run()
						{
							int nr;
							while (toolInvoker.readErrorLine() != null)
							{
								nr = workReporter.incrementAndGet();
								if (nr % 2 == 0)
									synchronized (monitor)
									{
										monitor.worked(1);
									}
							}
						}
					});
					stderrWatcher.start();
					stdoutWatcher.start();
					toolInvoker.waitForTool();
					if (tool == Tools.WMLINDENT &&
						selFile != null && targetPath == null)
					{
						EditorUtils.replaceEditorText(toolInvoker.getOutputContent());
					}

					if (tool == Tools.WMLSCOPE)
					{
						if (resource != null)
							resource.deleteMarkers(Constants.MARKER_WMLSCOPE, false, IResource.DEPTH_INFINITE);
						parseAndAddMarkers(toolInvoker.getOutputContent(), Constants.MARKER_WMLSCOPE);
					}
					else if (tool == Tools.WMLLINT)
					{
						if (resource != null)
							resource.deleteMarkers(Constants.MARKER_WMLLINT, false, IResource.DEPTH_INFINITE);
						parseAndAddMarkers(toolInvoker.getOutputContent(), Constants.MARKER_WMLLINT);
					}

					monitor.worked(50);
					monitor.done();
				}
				catch(Exception e)
				{
					Logger.getInstance().logException(e);
				}
				return Status.OK_STATUS;
			}
		};
		job.schedule();
	}

	/**
	 * Runs the wesnoth addon manager for uploading the specified container
	 * @param containerPath The container to upload
	 * @param stdout The array of streams where to output the stdout content
	 * @param stderr The array of streams where to output the stderr content
	 * @return  null if there were errors or an ExternalToolInvoker instance
	 */
	public static ExternalToolInvoker runWesnothAddonManager(String containerPath,
			OutputStream[] stdout, OutputStream[] stderr)
	{
        Paths paths = Preferences.getPaths( WesnothInstallsUtils.getInstallNameForResource( containerPath ) );
		if ( !ResourceUtils.isValidFilePath( containerPath ) ||
		     !checkWMLTool( paths, Tools.WESNOTH_ADDON_MANAGER.toString( ) ) )
			return null;

		File wmllintFile = new File(paths.getWMLToolsDir( )	+ "/wesnoth_addon_manager"); //$NON-NLS-1$
		List<String> arguments = new ArrayList<String>();
		arguments.add(wmllintFile.getAbsolutePath());

		if (!Preferences.getString(Constants.P_WAU_PASSWORD).isEmpty())
		{
			arguments.add("-P"); //$NON-NLS-1$
			arguments.add(Preferences.getString(Constants.P_WAU_PASSWORD));
		}

		if (Preferences.getBool(Constants.P_WAU_VERBOSE) == true)
			arguments.add("-V"); //$NON-NLS-1$

		arguments.add("-a"); //$NON-NLS-1$
		arguments.add(Preferences.getString(Constants.P_WAU_ADDRESS));

		arguments.add("-p"); //$NON-NLS-1$
		arguments.add(Preferences.getString(Constants.P_WAU_PORT));

		arguments.add("-u"); //$NON-NLS-1$
		arguments.add(containerPath);
		return runPythonScript(arguments, null, true, true, stdout, stderr);
	}

	/**
	 * Parses the output and adds markers accordingly
	 */
	public static void parseAndAddMarkers(String output, String markerType)
	{
		String[] lines = StringUtils.getLines(output);
		boolean startMD5 = false;

		for(String line : lines)
		{
			if (line.startsWith("#") || //$NON-NLS-1$
				line.matches("^[\\t ]*$") || //$NON-NLS-1$
				line.startsWith("wmllint:")) //$NON-NLS-1$
				continue;
			if (line.startsWith("%%")) //$NON-NLS-1$
			{
				startMD5 = !startMD5;
				continue;
			}
			// skip parsing collisions
			if (startMD5)
				continue;

			ResourceUtils.addMarkerForLine(line, markerType);
		}
	}

	/**
	 * Checks if the specified wmlTool is existing an can
	 * be runned (python path is valid aswell)
	 * @param paths The paths for current install
	 * @param wmlTool The wml tool to check
	 * @return True if tool is ok to be used
	 */
	public static boolean checkWMLTool( Paths paths, String wmlTool )
	{
	    if ( !ResourceUtils.isValidFilePath( Preferences.getString(Constants.P_PYTHON_PATH) ) )
        {
            GUIUtils.showWarnMessageBox( Messages.WMLTools_42 );
            return false;
        }

	    if ( wmlTool != null ) {
	        if ( paths.getWMLToolsDir( ).equals( "" ) ) //$NON-NLS-1$
            {
                GUIUtils.showWarnMessageBox( Messages.WMLTools_45 );
                return false;
            }
            File wmlToolFile = new File( paths.getWMLToolsDir( ) + wmlTool );

            if (!wmlToolFile.exists())
            {
                GUIUtils.showErrorMessageBox(String.format(Messages.WMLTools_47,
                        wmlToolFile));
                return false;
            }

            return true;
	    }

	    return false;
	}

	/**
	 * Runs a specified python script with the specified arguments
	 * (the call returns immediately)
	 *
	 * @param arguments the arguments of the "python" executable.
	 *        The first argument should be the script file name
	 * @param stdin A string that will be written to stdin of the python script
	 * @param stdout An array of streams where to write the stdout from the script.
	 * A non null array implies 'stdoutMonitoring' true.
	 * @param stderr An array of streams where to write the stderr from the script
	 * A non null array implies 'stderrMonitoring' true.
	 * @param stderrMonitoring True to start stderr monitoring on tool
	 * @param stdoutMonitoring True to start stdout monitoring on tool
	 * @return
	 */
	public static ExternalToolInvoker runPythonScript(List<String> arguments, String stdin,
			boolean stderrMonitoring, boolean stdoutMonitoring,
			final OutputStream[] stdout, final OutputStream[] stderr)
	{
		final ExternalToolInvoker pyscript = new ExternalToolInvoker(
				Preferences.getString(Constants.P_PYTHON_PATH) , arguments);

		pyscript.runTool();
		if (stderrMonitoring == true || stderr != null)
			pyscript.startErrorMonitor(stderr);
		if (stdoutMonitoring == true || stdout != null)
			pyscript.startOutputMonitor(stdout);
		if (stdin != null)
		{
			try
			{
				BufferedWriter stdinStream = new BufferedWriter(
						new OutputStreamWriter(pyscript.getStdin()));
				stdinStream.write(stdin);
				stdinStream.close();
			}
			catch (IOException e) {
				Logger.getInstance().logException(e);
			}
		}
		return pyscript;
	}

	/**
	 * The available tools enum
	 */
	public enum Tools
	{
		WMLLINT, WMLINDENT, WMLSCOPE, WESNOTH_ADDON_MANAGER;

		@Override
        public String toString()
		{
            return this.name( ).toLowerCase( Locale.ENGLISH );
		}
	}
}
