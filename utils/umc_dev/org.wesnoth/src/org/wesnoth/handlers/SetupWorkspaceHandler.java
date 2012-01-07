/*******************************************************************************
 * Copyright (c) 2010 - 2012 by Timotei Dolean <timotei21@gmail.com>
 * 
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;

import org.wesnoth.utils.WorkspaceUtils;

/**
 * Triggers the setup workspace routine ( guided )
 */
public class SetupWorkspaceHandler extends AbstractHandler
{
    @Override
    public Object execute( ExecutionEvent event )
    {
        WorkspaceUtils.setupWorkspace( true );
        return null;
    }
}
