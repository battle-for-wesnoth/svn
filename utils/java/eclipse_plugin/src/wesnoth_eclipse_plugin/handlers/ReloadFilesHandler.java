/*******************************************************************************
 * Copyright (c) 2010 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package wesnoth_eclipse_plugin.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

import wesnoth_eclipse_plugin.wizards.TemplateProvider;
import wesnoth_eclipse_plugin.wizards.generator.SchemaParser;

public class ReloadFilesHandler extends AbstractHandler
{
	@Override
	public Object execute(ExecutionEvent event) throws ExecutionException
	{
		SchemaParser.getInstance().parseSchema(true);
		TemplateProvider.getInstance().loadTemplates();
		return null;
	}
}
