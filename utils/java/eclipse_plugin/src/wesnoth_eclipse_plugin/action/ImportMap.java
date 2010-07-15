/*******************************************************************************
 * Copyright (c) 2010 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package wesnoth_eclipse_plugin.action;

import org.eclipse.jface.action.IAction;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.ui.IObjectActionDelegate;
import org.eclipse.ui.IWorkbenchPart;

import wesnoth_eclipse_plugin.globalactions.MapActions;

public class ImportMap implements IObjectActionDelegate
{
	public ImportMap()
	{
	}

	/**
	 * This method is runned *ONLY* if the user selected a "maps" folder
	 */
	@Override
	public void run(IAction action)
	{
		MapActions.importMap();
	}

	@Override
	public void selectionChanged(IAction action, ISelection selection){
	}

	@Override
	public void setActivePart(IAction action, IWorkbenchPart targetPart) {
	}
}
