/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 * 
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.preferences;

import org.eclipse.jface.preference.BooleanFieldEditor;

import org.wesnoth.Constants;
import org.wesnoth.Messages;
import org.wesnoth.WesnothPlugin;

public class AdvancedPreferencePage extends AbstractPreferencePage
{
    public AdvancedPreferencePage( )
    {
        super( GRID );

        setPreferenceStore( WesnothPlugin.getDefault( ).getPreferenceStore( ) );
        setDescription( Messages.AdvancedPreferencePage_0 );
    }

    @Override
    protected void createFieldEditors( )
    {
        addField( new BooleanFieldEditor( Constants.P_NO_TERRAIN_GFX,
            Messages.AdvancedPreferencePage_1,
            BooleanFieldEditor.SEPARATE_LABEL, getFieldEditorParent( ) ),
            Messages.AdvancedPreferencePage_2 );

        addField( new BooleanFieldEditor( Constants.P_WML_VALIDATION,
            "WML Validation, parent", BooleanFieldEditor.SEPARATE_LABEL,
            getFieldEditorParent( ) ),
            "If checked, the WML Editor will validate some of the "
                + "written WML to check for semantic errors." );
    }
}
