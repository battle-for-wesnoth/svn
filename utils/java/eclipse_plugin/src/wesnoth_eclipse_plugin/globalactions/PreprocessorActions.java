/*******************************************************************************
 * Copyright (c) 2010 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package wesnoth_eclipse_plugin.globalactions;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.Path;
import org.eclipse.ui.ide.IDE;

import wesnoth_eclipse_plugin.Constants;
import wesnoth_eclipse_plugin.Logger;
import wesnoth_eclipse_plugin.preferences.Preferences;
import wesnoth_eclipse_plugin.utils.ExternalToolInvoker;
import wesnoth_eclipse_plugin.utils.WorkspaceUtils;

public class PreprocessorActions
{
	/**
	 * preprocesses a file using the wesnoth's executable
	 * @param fileName the file to process
	 * @param targetDirectory target directory where should be put the results
	 * @param defines the list of additional defines to be added when preprocessing the file
	 * @param useThread true if the preprocessing should use a thread
	 * @param waitForIt true to wait for the preprocessing to finish
	 * @return
	 */
	public static boolean preprocessFile(String fileName, String targetDirectory,
			List<String> defines, boolean waitForIt)
	{
		try{
			List<String> arguments = new ArrayList<String>();

			if (defines != null && !defines.isEmpty())
			{
				String argument = "-p=";
				for(int i=0;i<defines.size()-1;i++)
				{
					argument += (defines.get(i) + ",");
				}
				argument  += defines.get(defines.size()-1);
			}
			else
			{
				arguments.add("-p");
			}
			arguments.add(fileName);
			arguments.add(targetDirectory);

			Logger.getInstance().log("preprocessing file: "+fileName);
			ExternalToolInvoker wesnoth = new ExternalToolInvoker(
					Preferences.getString(Constants.P_WESNOTH_EXEC_PATH),
					arguments);
			wesnoth.runTool();
			if (waitForIt)
				wesnoth.waitForTool();
			return true;
		}
		catch (Exception e) {
			Logger.getInstance().logException(e);
			return false;
		}
	}

	/**
	 * Opens the preprocessed version of the specified file
	 * @param file the file to show preprocessed output
	 * @param openPlain true if it should open the plain preprocessed version
	 * or false for the normal one
	 */
	public static void openPreprocessedFileInEditor(IFile file, boolean openPlain)
	{
		if (file == null || !file.exists())
		{
			Logger.getInstance().log("file null or non existent.",
					"The file is null or does not exist");
			return;
		}

		preprocessIfNotExists(file, true);

		IFileStore preprocFile =
			EFS.getLocalFileSystem().getStore(new Path(WorkspaceUtils.getTemporaryFolder()));
		preprocFile = preprocFile.getChild(file.getName() +
				(openPlain == true? ".plain" : ""));

		try
		{
			IDE.openEditorOnFileStore(WorkspaceUtils.getWorkbenchWindow().getActivePage(),
					preprocFile);
		}
		catch (Exception e)
		{
			Logger.getInstance().logException(e);
		}
	}

	public static String getPreprocessedFilePath(IFile file, boolean plain)
	{
		IFileStore preprocFile = EFS.getLocalFileSystem().getStore(new Path(WorkspaceUtils.getTemporaryFolder()));
		preprocFile = preprocFile.getChild(file.getName() + (plain == true? ".plain" : "") );
		return preprocFile.toString();
	}

	public static void preprocessIfNotExists(IFile file, boolean waitForIt)
	{
		if (new File(WorkspaceUtils.getTemporaryFolder() + file.getName()).exists())
			return;

		PreprocessorActions.preprocessFile(WorkspaceUtils.getPathRelativeToUserDir(file),
				WorkspaceUtils.getTemporaryFolder(), null, waitForIt);
	}
}
