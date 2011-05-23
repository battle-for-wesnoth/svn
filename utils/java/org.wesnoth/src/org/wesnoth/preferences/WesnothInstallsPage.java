/*******************************************************************************
 * Copyright (c) 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.preferences;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.Path;
import org.eclipse.jface.preference.DirectoryFieldEditor;
import org.eclipse.jface.preference.FieldEditor;
import org.eclipse.jface.preference.FileFieldEditor;
import org.eclipse.jface.preference.StringFieldEditor;
import org.eclipse.jface.util.PropertyChangeEvent;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ITableLabelProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TableViewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.FocusEvent;
import org.eclipse.swt.events.FocusListener;
import org.eclipse.swt.events.ModifyEvent;
import org.eclipse.swt.events.ModifyListener;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.events.SelectionAdapter;
import org.eclipse.swt.events.SelectionEvent;
import org.eclipse.swt.events.VerifyEvent;
import org.eclipse.swt.events.VerifyListener;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Combo;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Table;
import org.eclipse.swt.widgets.TableColumn;
import org.eclipse.swt.widgets.Text;
import org.eclipse.xtext.ui.editor.preferences.fields.LabelFieldEditor;
import org.wesnoth.Constants;
import org.wesnoth.Logger;
import org.wesnoth.Messages;
import org.wesnoth.WesnothPlugin;
import org.wesnoth.templates.ReplaceableParameter;
import org.wesnoth.templates.TemplateProvider;
import org.wesnoth.utils.StringUtils;

public class WesnothInstallsPage extends AbstractPreferencePage
{
    private Text                    txtInstallName_;
    private Combo                   cmbVersion_;

    private List<WesnothInstall>     installs_;
    private Table                   installsTable_;
    private TableViewer             installsTableViewer_;


    private DirectoryFieldEditor    wmlToolsField_;
    private DirectoryFieldEditor    wesnothWorkingDirField_;
    private DirectoryFieldEditor    wesnothUserDirField_;
    private FileFieldEditor         wesnothExecutableField_;

    private List<String>            wmlToolsList_;
    private Composite               parentComposite_;

    public WesnothInstallsPage()
    {
        super(GRID);
        setPreferenceStore(WesnothPlugin.getDefault().getPreferenceStore());
        setTitle("Wesnoth Installs Preferences");

        wmlToolsList_ = new ArrayList<String>();
        wmlToolsList_.add("wmllint"); //$NON-NLS-1$
        wmlToolsList_.add("wmlindent"); //$NON-NLS-1$
        wmlToolsList_.add("wmlscope"); //$NON-NLS-1$
        wmlToolsList_.add("wesnoth_addon_manager"); //$NON-NLS-1$

        installs_ = new ArrayList<WesnothInstall>();
        // add the default install first
        installs_.add(new WesnothInstall("Default", "")); //$NON-NLS-1$ //$NON-NLS-2$

        // unpack installs
        String[] installs = Preferences.getString(Constants.P_INST_INSTALL_LIST).split(";");
        for ( String str : installs ){
            if (str.isEmpty())
                continue;

            String[] tokens = str.split(":");

            if ( tokens.length != 2 ) {
                Logger.getInstance().logError( "invalid install [" + str + "] in installs list." );
                continue;
            }
            installs_.add( new WesnothInstall( tokens[0], tokens[1] ) );
        }
    }

    @Override
    protected void createFieldEditors()
    {
        ModifyListener listener = new ModifyListener() {

            @Override
            public void modifyText(ModifyEvent e)
            {
                checkState();
                guessDefaultPaths();
            }
        };

        wesnothExecutableField_ = new FileFieldEditor(Constants.P_WESNOTH_EXEC_PATH,
                Messages.WesnothPreferencesPage_5, getFieldEditorParent());
        wesnothExecutableField_.getTextControl(getFieldEditorParent()).
            addFocusListener(new FocusListener() {
                @Override
                public void focusLost(FocusEvent e)
                {
                    checkState();
                    String wesnothExec = wesnothExecutableField_.getStringValue();
                    if (wesnothWorkingDirField_.getStringValue().isEmpty() &&
                        !wesnothExec.isEmpty() &&
                        new File(wesnothExec.substring(0,
                                wesnothExec.lastIndexOf(new File(wesnothExec).getName()))).exists())
                    {
                        wesnothWorkingDirField_.setStringValue(wesnothExec.substring(0,
                                wesnothExec.lastIndexOf(new File(wesnothExec).getName()))
                        );
                    }
                }
                @Override
                public void focusGained(FocusEvent e)
                {
                }
            });
        addField(wesnothExecutableField_, Messages.WesnothPreferencesPage_6);

        wesnothWorkingDirField_ = new DirectoryFieldEditor(Constants.P_WESNOTH_WORKING_DIR,
                Messages.WesnothPreferencesPage_7, getFieldEditorParent());
        wesnothWorkingDirField_.getTextControl(getFieldEditorParent()).
            addModifyListener(listener);
        addField(wesnothWorkingDirField_, Messages.WesnothPreferencesPage_8);

        wesnothUserDirField_ = new DirectoryFieldEditor(Constants.P_WESNOTH_USER_DIR,
                Messages.WesnothPreferencesPage_9, getFieldEditorParent());
        addField(wesnothUserDirField_, Messages.WesnothPreferencesPage_10);

        wmlToolsField_ = new DirectoryFieldEditor(Constants.P_WESNOTH_WMLTOOLS_DIR,
                Messages.WesnothPreferencesPage_11, getFieldEditorParent());
        addField(wmlToolsField_, Messages.WesnothPreferencesPage_12);

        addField(new FileFieldEditor(Constants.P_PYTHON_PATH, Messages.WesnothPreferencesPage_13, getFieldEditorParent()));

        addField(new LabelFieldEditor(Messages.WesnothPreferencesPage_14, getFieldEditorParent()));

        guessDefaultPaths();
    }

    @Override
    protected Control createContents(Composite parent)
    {
        Composite installComposite = new Composite(parent, 0);
        installComposite.setLayout(new GridLayout(2, false));
        installComposite.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true, 1, 1));

        // create install manager
        installsTableViewer_ = new TableViewer(installComposite, SWT.BORDER | SWT.FULL_SELECTION);
        installsTable_ = installsTableViewer_.getTable();
        installsTable_.addMouseListener(new MouseAdapter() {
            @Override
            public void mouseDown(MouseEvent e)
            {
                updateInterface(getSelectedInstall());
            }
        });
        installsTable_.setHeaderVisible(true);
        installsTable_.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true, 1, 1));

        TableColumn tblclmnName = new TableColumn(installsTable_, SWT.NONE);
        tblclmnName.setWidth(150);
        tblclmnName.setText("Install Name");

        TableColumn tblclmnWesnothVersion = new TableColumn(installsTable_, SWT.NONE);
        tblclmnWesnothVersion.setWidth(100);
        tblclmnWesnothVersion.setText("Wesnoth version");

        installsTableViewer_.setContentProvider(new ContentProvider());
        installsTableViewer_.setLabelProvider(new TableLabelProvider());
        installsTableViewer_.setInput(installs_);

        Composite composite = new Composite(installComposite, SWT.NONE);
        FillLayout fl_composite = new FillLayout(SWT.VERTICAL);
        fl_composite.spacing = 10;
        fl_composite.marginHeight = 10;
        composite.setLayout(fl_composite);
        GridData gd_composite = new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1);
        gd_composite.widthHint = 23;
        composite.setLayoutData(gd_composite);

        Button btnAdd = new Button(composite, SWT.NONE);
        btnAdd.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                addInstall();
            }
        });
        btnAdd.setText("Add");

        Button btnRemove = new Button(composite, SWT.NONE);
        btnRemove.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                removeInstall();
            }
        });
        btnRemove.setText("Remove");

        Button btnSetAsDefault = new Button(composite, SWT.NONE);
        btnSetAsDefault.addSelectionListener(new SelectionAdapter() {
            @Override
            public void widgetSelected(SelectionEvent e) {
                setInstallAsDefault();
            }
        });
        btnSetAsDefault.setText("Set as default");

        Label lblInstallName = new Label(parent, SWT.NONE);
        lblInstallName.setText("Install name:");

        txtInstallName_ = new Text(parent, SWT.SINGLE);
        txtInstallName_.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, false, false, 1, 1));
        txtInstallName_.addVerifyListener(new VerifyListener() {

            @Override
            public void verifyText(VerifyEvent e)
            {
                e.doit = ( e.character >= 'a' && e.character <= 'z' ) ||
                         ( e.character >= 'A' && e.character <= 'Z' ) ||
                         ( e.character >= '0' && e.character <= '9' ) ||
                         e.keyCode == SWT.BS ||
                         e.keyCode == SWT.ARROW_LEFT ||
                         e.keyCode == SWT.ARROW_RIGHT ||
                         e.keyCode == SWT.DEL;

                if ( (txtInstallName_.getText() + e.character).equalsIgnoreCase("default"))
                    e.doit = false;
            }
        });

        Label lblVersion = new Label(parent, SWT.NONE);
        lblVersion.setText("Wesnoth Version:");

        cmbVersion_ = new Combo(parent, SWT.READ_ONLY);
        cmbVersion_.setLayoutData(new GridData(SWT.FILL, SWT.CENTER, true, false, 1, 1));

        cmbVersion_.add("1.9.x"); //$NON-NLS-1$
        cmbVersion_.add("trunk"); //$NON-NLS-1$

        // create fields
        parentComposite_ = (Composite) super.createContents(parent);

        return parentComposite_;
    }

    protected void addInstall()
    {
        updateInterface(null);
    }

    protected void setInstallAsDefault()
    {
        WesnothInstall install = getSelectedInstall();
        if (install != null) {
            Preferences.getPreferences().setValue(Constants.P_INST_DEFAULT_INSTALL, install.Name);
            installs_.get(0).Version = install.Version;
        }
    }

    protected void removeInstall()
    {
        WesnothInstall install = getSelectedInstall();
        if (install != null && install.Name.equalsIgnoreCase("default")){
            installs_.remove(installsTable_.getSelectionIndex());
            installsTableViewer_.refresh();
        }
    }

    private WesnothInstall getSelectedInstall()
    {
        if (installsTable_.getSelectionIndex() == -1)
            return null;
        return installs_.get(installsTable_.getSelectionIndex());
    }

    private void updateInterface(WesnothInstall install)
    {
        txtInstallName_.setText( install == null ? "" : install.Name );
        txtInstallName_.setEditable( install == null ? true : false );

    }

    @Override
    protected void checkState()
    {
        super.checkState();
        setValid(true);
        testWMLToolsPath(wmlToolsField_.getStringValue());
        setErrorMessage(null);
    }

    /**
     * Tries the list of available paths for current os
     */
    private void guessDefaultPaths()
    {
        String os = "linux"; //$NON-NLS-1$
        if (Constants.IS_MAC_MACHINE)
            os = "mac"; //$NON-NLS-1$
        else if (Constants.IS_WINDOWS_MACHINE)
            os = "windows"; //$NON-NLS-1$

        List<ReplaceableParameter> params = new ArrayList<ReplaceableParameter>();
        params.add(new ReplaceableParameter("$$home_path", System.getProperty("user.home"))); //$NON-NLS-1$ //$NON-NLS-2$

        testPaths(StringUtils.getLines(
                TemplateProvider.getInstance().getProcessedTemplate(os + "_exec", params)), //$NON-NLS-1$
                wesnothExecutableField_);
        testPaths(StringUtils.getLines(
                TemplateProvider.getInstance().getProcessedTemplate(os + "_data", params)), //$NON-NLS-1$
                wesnothWorkingDirField_);
        testPaths(StringUtils.getLines(
                TemplateProvider.getInstance().getProcessedTemplate(os + "_user", params)), //$NON-NLS-1$
                wesnothUserDirField_);

        // guess the working dir based on executable's path
        Text textControl = wesnothWorkingDirField_.getTextControl(
                getFieldEditorParent());

        String wesnothExec = wesnothExecutableField_.getStringValue();
        if (wesnothWorkingDirField_.getStringValue().isEmpty() &&
            !wesnothExec.isEmpty() &&
            new File(wesnothExec.substring(0,
                    wesnothExec.lastIndexOf(new File(wesnothExec).getName()))).exists())
        {
            textControl.setText(wesnothExec.substring(0,
                    wesnothExec.lastIndexOf(new File(wesnothExec).getName()))
            );
        }

        // guess the wmltools path
        if (wmlToolsField_.getStringValue().isEmpty() &&
            !wesnothWorkingDirField_.getStringValue().isEmpty())
        {
            String path = wesnothWorkingDirField_.getStringValue() + "/data/tools"; //$NON-NLS-1$
            if (testWMLToolsPath(path))
                wmlToolsField_.setStringValue(path);
        }

        // guess the userdata path
        if (wesnothUserDirField_.getStringValue().isEmpty() &&
            !wesnothWorkingDirField_.getStringValue().isEmpty())
        {
            String path = wesnothWorkingDirField_.getStringValue() + "/userdata"; //$NON-NLS-1$
            testPaths(new String[] { path },wesnothUserDirField_);
        }

        checkState();
    }

    /**
     * Tests for wmltools in the specified path
     * @param path
     * @return
     */
    private boolean testWMLToolsPath(String path)
    {
        for(String tool : wmlToolsList_)
        {
            if (!(new File(path + Path.SEPARATOR + tool).exists()))
            {
                setErrorMessage(String.format(Messages.WesnothPreferencesPage_24,
                        tool));
                return false;
            }
        }
        return true;
    }

    /**
     * Tests the list of paths and if any path exists it will
     * set it as the string value to the field editor
     * if the field editor value is empty
     * @param list The list to search in
     * @param field The field to put the path in
     */
    private void testPaths(String[] list, StringFieldEditor field)
    {
        if (!(field.getStringValue().isEmpty()))
            return;

        for(String path : list)
        {
            if (new File(path).exists())
            {
                field.setStringValue(path);
                return;
            }
        }
    }

    /**
     * This method will unset invalid properties's values,
     * and saving only valid ones.
     */
    private void savePreferences()
    {
        if (!wesnothExecutableField_.isValid())
            wesnothExecutableField_.setStringValue(""); //$NON-NLS-1$
        if (!wesnothUserDirField_.isValid())
            wesnothUserDirField_.setStringValue(""); //$NON-NLS-1$
        if (!wesnothWorkingDirField_.isValid())
            wesnothWorkingDirField_.setStringValue(""); //$NON-NLS-1$
        if (!wmlToolsField_.isValid())
            wmlToolsField_.setStringValue(""); //$NON-NLS-1$

        // pack back the installs
        String installs = "";
        for ( WesnothInstall install : installs_ ){
            // don't save the default install
            if ( install.Name.equals("Default") )
                continue;

            if (installs.isEmpty() == false)
                installs += ";";

            installs += install.Name + ":" + install.Version;
        }
        Preferences.getPreferences().setValue(Constants.P_INST_INSTALL_LIST, installs);
    }

    @Override
    protected void performApply()
    {
        savePreferences();
        super.performApply();
    }

    @Override
    public boolean performOk()
    {
        savePreferences();
        return super.performOk();
    }

    @Override
    public void propertyChange(PropertyChangeEvent event)
    {
        super.propertyChange(event);
        if (event.getProperty().equals(FieldEditor.VALUE))
            checkState();
    }

    private static class ContentProvider extends ArrayContentProvider {
        @Override
        public Object[] getElements(Object inputElement)
        {
            return super.getElements(inputElement);
        }
    }

    private class TableLabelProvider extends LabelProvider implements ITableLabelProvider {
        public Image getColumnImage(Object element, int columnIndex) {
            return null;
        }
        public String getColumnText(Object element, int columnIndex) {
            if (element instanceof WesnothInstall){

                if (columnIndex == 0){ // name
                   return ((WesnothInstall)element).Name;
                }else if (columnIndex == 1){ // version
                    return ((WesnothInstall)element).Version;
                }
            }
            return "";
        }
    }

    public static class WesnothInstall
    {
        public String Name;
        public String Version;

        public WesnothInstall(String name, String version)
        {
            Name = name;
            Version = version;
        }
    }
}
