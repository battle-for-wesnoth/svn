/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.ui.labeling.wmldoc;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.StyleRange;
import org.eclipse.swt.widgets.Display;
import org.wesnoth.schema.SchemaParser;
import org.wesnoth.ui.Messages;
import org.wesnoth.wml.WMLTag;

/**
 * Displays wml doc for a tag
 * [tag] or [/tag]
 */
public class WMLDocTag implements IWMLDocProvider
{
	private WMLTag tag_;
	private String title_;
	private String contents_;
	private List<StyleRange> styleRanges_;

	public WMLDocTag( String installName, String name )
	{
		tag_ = SchemaParser.getInstance( installName ).getTags().get(name);
	}

	/**
	 * A method used for lazly generating the documentation
	 */
	private void generateDoc()
	{
		if (title_ != null && contents_ != null && styleRanges_ != null)
			return;

		styleRanges_ = new ArrayList<StyleRange>();

		if (tag_ == null)
			return;

		title_ = Messages.WMLDocTag_0 + tag_.getName() + "':"; //$NON-NLS-1$

		StringBuilder content = new StringBuilder();
		if ( !tag_.get_Description().isEmpty( ) )
		{
			content.append(Messages.WMLDocTag_1);
			addStyleRange(0, content.length() - 1, SWT.BOLD);
			content.append( tag_.get_Description() ); //$NON-NLS-1$
		}
		contents_ = content.toString();
	}

	/** Adds a style range to current list
	 *
	 * @param offset
	 * @param length
	 * @param style
	 */
	private void addStyleRange(int offset, int length, int style)
	{
		styleRanges_.add(new StyleRange(offset, length,
				Display.getDefault().getSystemColor(SWT.COLOR_INFO_FOREGROUND),
				Display.getDefault().getSystemColor(SWT.COLOR_INFO_BACKGROUND),
				style));
	}

	public String getTitle()
	{
		generateDoc();
		return title_;
	}

	public String getInfoText()
	{
		return ""; //$NON-NLS-1$
	}

	public String getContents()
	{
		generateDoc();
		return contents_;
	}
	public StyleRange[] getStyleRanges()
	{
		generateDoc();
		return styleRanges_.toArray(new StyleRange[styleRanges_.size()]);
	}
}
