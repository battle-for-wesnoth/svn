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

import org.eclipse.core.resources.IFile;

import wesnoth_eclipse_plugin.Constants;
import wesnoth_eclipse_plugin.Logger;
import wesnoth_eclipse_plugin.builder.ExternalToolInvoker;
import wesnoth_eclipse_plugin.preferences.Preferences;

public class EditorActions
{
	public static void startEditor(IFile file)
	{
		if (file == null || file.exists())
		{
			Logger.getInstance().log("non-existing map file",
					"Please select an existing map file before opening it.");
			return;
		}

		startEditor(file.getLocation().toOSString());
	}

	public static void startEditor(String mapName)
	{
		String editorPath = Preferences.getString(Constants.P_WESNOTH_EXEC_PATH);
		String workingDir = Preferences.getString(Constants.P_WESNOTH_WORKING_DIR);

		if (workingDir.isEmpty())
			workingDir = editorPath.substring(0, editorPath.lastIndexOf(new File(editorPath).getName()));

		if (editorPath.isEmpty())
		{
			Logger.getInstance().log("wesnoth executable not set (startEditor)",
					"Please set the wesnoth's executable path first.");
			return;
		}

		Logger.getInstance().log(String.format("Running: [%s] with args: %s\n",
				editorPath, getLaunchEditorArguments(mapName, workingDir)));
		ExternalToolInvoker.launchTool(editorPath, getLaunchEditorArguments(mapName, workingDir),
				Constants.TI_SHOW_OUTPUT_USER | Constants.TI_SHOW_OUTPUT, true);
	}

	public static List<String> getLaunchEditorArguments(String mapName, String workingDir)
	{
		List<String> args = new ArrayList<String>(3);

		args.add("-e");
		args.add(mapName);

		if (!workingDir.isEmpty())
		{
			args.add("--data-dir");
			args.add(workingDir);
		}

		return args;
	}
}
