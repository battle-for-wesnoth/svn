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

import wesnoth_eclipse_plugin.utils.WMLTools;
import wesnoth_eclipse_plugin.utils.WMLTools.Tools;

public class RunWMLIndentOnSelection extends ObjectActionDelegate
{
	@Override
	public void run(IAction action)
	{
		WMLTools.runWMLToolAsWorkspaceJob(Tools.WMLINDENT, null);
	}
}
